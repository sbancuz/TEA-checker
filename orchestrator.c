#define _GNU_SOURCE
#include <sched.h>
#include <stdlib.h>
#define ORCHESTRATOR
#include "modules/tester.h"
#include "sys/stat.h"
#include <dlfcn.h>
#include <fcntl.h>
#include <getopt.h>
#include <linux/sched.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#define IMPLEMENTATIONS
#include "libs/lbstd.h"

#define JIMP_IMPLEMENTATION
#include "libs/jim/jimp.h"

// TODO: Find a better way to communicate the result back to the orchestrator
// such that we don't need to recompile
#include "include/delim.h"
#include "modules/commands.h"

#define DEFAULT_STR_MAX_SIZE 32

#define EACH_RUNNER(X)                                                         \
  X(RUNNER_KERNEL)                                                             \
  X(RUNNER_USER)                                                               \
  X(RUNNER_SIMULATION)                                                         \
  X(RUNNER_NUM)

typedef_enum(runner_t, EACH_RUNNER);

#define EACH_SIMULATION_IMPL(X)                                                \
  X(SIM_CHIPYARD)                                                              \
  X(SIM_NUM)

typedef_enum(simulation_impl_t, EACH_SIMULATION_IMPL);

#define EACH_TARGET(X)                                                         \
  X(TARGET_RISCV)                                                              \
  X(TARGET_X86_64)                                                             \
  X(TARGET_NUM)

#define get_func(lib, func)                                                    \
  ({                                                                           \
    char *f = (func);                                                          \
    void *fn = dlsym(lib, f);                                                  \
    if (!fn) {                                                                 \
      plog(ERR, "Failed to get diagnostic function %s\n", dlerror());          \
      dlclose(lib);                                                            \
    }                                                                          \
    fn;                                                                        \
  })

typedef_enum(target_t, EACH_TARGET);

#define SILENCE_WARNINGS                                                       \
  "-Wno-attributes", "-Wno-cpp", "-Wno-unused-parameter",                      \
      "-fno-optimize-sibling-calls"

const char *module_dir = "modules";
const char *json = "module.json";
const char *include_dir = "../../include/";
const char *include_dir_name = "include/";

char *cwd;
char *kernel_header_dir;

bool running_all = false;

typedef struct {
  target_t target;
  runner_t runner;
  simulation_impl_t sim_impl;
  cpuid_t cpu;
  u64 clock_speed;

  bool run_as_exe;
  const char *to_mitigate;
  bool save;
  const char *save_file_name;

  struct {
    const char *shell;

    struct {
      const char *directory;
    } chipyard;
  } extra_sim_options;
} run_options_t;

typedef struct {
  const char *module_name;
  const char *module_path;
  const char *test_file;
  da(const char *) sources;
  da(const char *) depends_on;

  run_options_t opts;

  bool mitigate;
  result_code_t result_code;
  void *result;
  usize result_size;
} test_t;

typedef struct {
  void *shlib;

  bool (*setup)(request_dependencies_t *);
  u64 (*get_result_size)();
  result_code_t (*get_result_diagnostics)(request_return_t *);
} manager_t;

da(test_t) runned_test = {0};

int test_cmp(test_t t1, test_t t2);
test_t *test_find(const char *module_name, const target_t taget,
                  const runner_t runner, const cpuid_t cpu);
test_t *test_new(const char *module_name, run_options_t opts);
void test_free(test_t *test);
bool test_save(test_t *test, str *sink);

bool get_config_for_module(test_t out[static 1]);
bool parse_test(Jimp jimp[static 1], test_t out[static 1]);
bool save_run(const char *path);
bool load_run(const char *path);

const char *make_shared_lib(cmd_t *c, const char *name, int n, bool mitigate,
                            const char *sources[static n]);
bool compile_user_module(cmd_t c[static 1], test_t *test);
bool compile_kernel_module(cmd_t c[static 1], test_t *test);
bool compile_simulation_module(cmd_t c[static 1], test_t *test);
bool compile_kmod(cmd_t c[static 1], const char mkfile[static 1],
                  const char kmod_dir[static 1]);
bool load_kmod(cmd_t c[static 1], test_t *test);
bool unload_kmod(cmd_t c[static 1], test_t *test);
bool compile_test(cmd_t c[static 1], test_t *t);

bool get_manager(manager_t out[static 1], test_t t[static 1]);

bool execute_dependencies(test_t *parent);
bool execute_dependency(cmd_t cmd[static 1], test_t *test);

result_code_t run_user_test(cmd_t *c, test_t *t,
                            struct run_function_request req, manager_t a);
result_code_t run_kernel_test(cmd_t *c, test_t *t,
                              struct run_function_request req, manager_t a);
result_code_t run_simulation_test(cmd_t *c, test_t *t,
                                  struct run_function_request req, manager_t a);
result_code_t run_test(cmd_t *c, test_t *t, struct run_function_request r,
                       manager_t a);

void serialize_args(str *out, struct run_function_request req);
void args_to_c_array(str *out, struct run_function_request req);
strv parse_between_delim(u8 *buf, usize buflen, char *delim, usize delim_len);

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

int test_cmp(test_t t1, test_t t2) {
  int res = 0;
  res = strcmp(t1.module_name, t2.module_name);
  if (res != 0)
    return res;

  res = (t1.opts.target != t2.opts.target) + (t1.opts.cpu != t2.opts.cpu) +
        (t1.opts.runner != t2.opts.runner);

  return res;
}

test_t *test_find(const char *module_name, const target_t taget,
                  const runner_t runner, const cpuid_t cpu) {
  da_foreach(test_t, t1, &runned_test) {
    test_t t2 = {
        .module_name = module_name,
        .opts =
            {
                .runner = runner,
                .target = taget,
                .cpu = cpu,
            },
    };

    if (test_cmp(*t1, t2) == 0)
      return t1;
  }

  return NULL;
}

test_t *test_new(const char *module_name, run_options_t opts) {

  // Don't inherit
  opts.run_as_exe = false;
  test_t t = {
      .module_name = module_name,
      .opts = opts,
      .result_code = false,
  };

  if (!get_config_for_module(&t)) {
    plog(ERR, "Could not get config for %s", t.module_name);
    return NULL;
  }

  t.module_path = strdup(tsprintf("%s/%s/%s", cwd, module_dir, t.module_name));

  return da_append(&runned_test, t);
}

void test_free(test_t *t) {
  if (!t)
    return;

  /* free((char *)t->module_name); */
  free((char *)t->module_path);
  free((char *)t->test_file);

  for (usize i = 0; i < t->sources.count; i++)
    free((char *)t->sources.items[i]);

  for (usize i = 0; i < t->depends_on.count; i++)
    free((char *)t->depends_on.items[i]);

  da_free(&t->sources);
  da_free(&t->depends_on);

  free(t->result);
}

bool test_save(test_t *t, str *sink) {
  if (!t) {
    plog(WARN, "Test doesn't exist passing...");
    return false;
  }

  str_append_cstr(sink, t->module_name);
  da_append(sink, '\0');

#define serialize_field(sink, field)                                           \
  da_append_many(sink, (u8 *)&field, sizeof(field));

  serialize_field(sink, t->opts.cpu);
  serialize_field(sink, t->result_code);
  serialize_field(sink, t->opts.runner);
  serialize_field(sink, t->opts.target);
  serialize_field(sink, t->result_size);

  da_append_many(sink, t->result, t->result_size);

  return true;
}

bool save_run(const char *path) {
  str out_file = {};

  da_append_many(&out_file, &runned_test.count, sizeof(runned_test.count));

  da_foreach_s(test_t, t, runned_test) {
    if (!test_save(t, &out_file)) {
      plog(ERR, "Failed to save %s", t->module_name);
      return false;
    }
  };

  write_to_file_bin(path, (u8 *)out_file.items, out_file.count);
  da_free(&out_file);

  return true;
}

bool load_run(const char *path) {
  str file = {};

  if (!read_file(path, &file)) {
    plog(ERR, "Error reading file %s", path);

    return false;
  }

  u8 *ptr = (u8 *)file.items;
  int num_tests = bp_get_usize(&ptr);
  plog(INFO, "num tests %d", num_tests);

  for (int i = 0; i < num_tests; i++) {
    const char *module_name = bp_get_string(&ptr);
    cpuid_t cpu = bp_get_int(&ptr);
    result_code_t result_code = bp_get_int(&ptr);
    runner_t runner = bp_get_int(&ptr);
    target_t target = bp_get_int(&ptr);
    usize result_size = bp_get_usize(&ptr);
    /* plog(INFO, "res_size %d", result_size); */
    void *result = bp_get_bytes(&ptr, result_size);
    // TODO: SEE FOR SIM
    test_t *t = test_new(
        module_name,
        (run_options_t){.runner = runner, .target = target, .cpu = cpu});

    t->result_size = result_size;
    t->result_code = result_code;

    t->result = malloc(t->result_size);
    memset(t->result, 0, t->result_size);

    t->module_name = strdup(module_name);
  }

  da_free(&file);

  return false;
}

int find_modules(const char *base_dir, char ***out_dirs) {
  DIR *dir;
  struct dirent *entry;
  char **results = NULL;
  int count = 0;

  dir = opendir(base_dir);
  if (!dir) {
    return -1;
  }

  while ((entry = readdir(dir)) != NULL) {
    if (entry->d_name[0] == '.')
      continue;

    char subdir_path[PATH_MAX];
    snprintf(subdir_path, sizeof(subdir_path), "%s/%s", base_dir,
             entry->d_name);

    struct stat st;
    if (stat(subdir_path, &st) != 0)
      continue;

    if (!S_ISDIR(st.st_mode))
      continue;

    char module_path[PATH_MAX];
    snprintf(module_path, sizeof(module_path), "%s/module.json", subdir_path);

    if (access(module_path, F_OK) == 0) {
      char **tmp = realloc(results, sizeof(char *) * (count + 1));
      if (!tmp) {
        closedir(dir);
        return -1;
      }
      results = tmp;
      results[count] = strdup(entry->d_name);
      count++;
    }
  }

  closedir(dir);
  *out_dirs = results;
  return count;
}

void get_timestamp_utc(char *buffer, size_t size) {
  time_t now = time(NULL);
  struct tm tm_info;

  localtime_r(&now, &tm_info);
  strftime(buffer, size, "%Y-%m-%dT%H:%M:%SZ", &tm_info);
}

bool parse_test(Jimp jimp[static 1], test_t out[static 1]) {
  if (!jimp_object_begin(jimp)) {
    return false;
  }

  while (jimp_object_member(jimp)) {

    if (memcmp(jimp->string, "test_file", sizeof("test_file")) == 0) {
      if (!jimp_string(jimp))
        return false;

      // TODO: Maybe don't use strdup
      out->test_file = strdup(jimp->string);
    }

    if (memcmp(jimp->string, "run_as_exe", sizeof("run_as_exe")) == 0) {
      if (!jimp_bool(jimp))
        return false;

      out->opts.run_as_exe = jimp->boolean;
    }

    if (memcmp(jimp->string, "sources", sizeof("sources")) == 0) {
      if (!jimp_array_begin(jimp))
        return false;

      for (int i = 0; jimp_array_item(jimp); i++) {
        if (!jimp_string(jimp))
          return false;

        da_append(&out->sources, strdup(jimp->string));
      }

      if (!jimp_array_end(jimp))
        return false;
    }

    if (memcmp(jimp->string, "depends_on", sizeof("depends_on")) == 0) {
      if (!jimp_array_begin(jimp))
        return false;

      for (int i = 0; jimp_array_item(jimp); i++) {
        if (!jimp_string(jimp))
          return false;

        da_append(&out->depends_on, strdup(jimp->string));
      }

      if (!jimp_array_end(jimp))
        return false;
    }
  }

  if (!jimp_object_end(jimp)) {
    return false;
  }

  if (out->opts.to_mitigate[0]) {
    str mitigate_copy = {0};
    str_append_cstr(&mitigate_copy, out->opts.to_mitigate);
    da_append(&mitigate_copy, '\0');
    char *token = strtok(mitigate_copy.items, ",");
    while (token) {
      if (!strcmp(token, "all") || !strcmp(token, out->module_name)) {
        out->mitigate = true;
        break;
      }
      token = strtok(NULL, ",");
    }
    da_free(&mitigate_copy);
  }

  da_append(&out->depends_on, strdup("root"));
  da_append(&out->sources, strdup(tsprintf("%s.c", out->test_file)));

  return true;
}

const char *make_shared_lib(cmd_t *c, const char *name, int n, bool mitigate,
                            const char *sources[static n]) {

  const char *out = tsprintf("%s.so", name);

  // TODO: Maybe include also the modules?
  cmd_append(c, __BEAR "gcc", SILENCE_WARNINGS, "-g3", "-shared", "-fno-plt",
             "-O0", "-fPIC", "-march=native", tsprintf("-I%s", include_dir),
             "-o", out);

  if (mitigate) {
    cmd_append(c, "-DMITIGATE");
  }

  for (int i = 0; i < n; i++) {
    cmd_append(c, sources[i]);
  }

  return out;
}

bool get_manager(manager_t out[static 1], test_t t[static 1]) {
  const char *in = tsprintf("./%s_manager.c", t->module_name);

  cmd_t c = {0};
  const char *so = make_shared_lib(&c, in, 1, false, (const char *[]){in});
  if (!cmd_run_reset(&c)) {
    plog(ERR, "Failed to compile the shared library %s: %s\n", in,
         strerror(errno));

    return false;
  }
  cmd_free(&c);

  out->shlib = dlopen(so, RTLD_LAZY);
  if (out->shlib == NULL) {
    plog(ERR, "Failed to open the shared library %s\n", dlerror());

    return false;
  }

  out->setup =
      get_func(out->shlib, tsprintf("%s_result_setup", t->module_name));
  out->get_result_size =
      get_func(out->shlib, tsprintf("%s_result_size", t->module_name));
  out->get_result_diagnostics =
      get_func(out->shlib, tsprintf("%s_result_diagnostics", t->module_name));

  return true;
}

bool get_config_for_module(test_t out[static 1]) {
  Jimp jimp = {0};

  char *config_path = tsprintf("%s/%s/%s", module_dir, out->module_name, json);

  str file = {0};
  read_file(config_path, &file);

  jimp_begin(&jimp, config_path, file.items, file.count);

  if (!parse_test(&jimp, out)) {
    plog(ERR, "could not read the config file for moodule %s",
         out->module_name);

    goto exit_file;
  }

  da_free(&file);
  free(jimp.string);
  return true;

exit_file:
  da_free(&file);
  free(jimp.string);

  return false;
}

const char *test_define_name_templ = "#ifndef _TEST_NAME\n"
                                     "#define _TEST_NAME\n"
                                     "#define TEST_NAME %s\n"
                                     "#define TEST_NAME_STR \"%s\"\n"
                                     "#endif // _TEST_NAME\n";

bool compile_user_module(cmd_t c[static 1], test_t *test) {

  if (test->opts.run_as_exe) {
    const char *out = tsprintf("%s", test->module_name);

    // TODO: Maybe include also the modules?
    cmd_append(c, __BEAR "gcc", "-march=native", SILENCE_WARNINGS, "-ggdb",
               "-O0", "-fno-plt", tsprintf("-I%s", include_dir), "-o", out);

    for (int i = 0; i < test->sources.count; i++) {
      cmd_append(c, test->sources.items[i]);
    }

    if (test->mitigate) {
      cmd_append(c, "-DMITIGATE");
    }
    cmd_append(c, "-DEXE");
  } else {
    make_shared_lib(c, test->module_name, test->sources.count, test->mitigate,
                    test->sources.items);
  }

  // TODO: Review this immintr stuff
  /* cmd_append(c, tsprintf("%s/%s_immintr.S", include_dir, */
  /*                        target_t_strs[test->opts.target])); */

  cmd_append(c, tsprintf("-D%s", target_t_strs[test->opts.target]),
             tsprintf("-D%s", runner_t_strs[test->opts.runner]));

  if (!cmd_run_reset(c)) {
    plog(ERR, "could not compile user module");
    return false;
  }

  return true;
}

bool compile_kmod(cmd_t c[static 1], const char mkfile[static 1],
                  const char kmod_dir[static 1]) {
  write_to_file("Makefile", mkfile);

  cmd_append(c, __BEAR "make", "-C", kernel_header_dir, kmod_dir, "modules",
             "V=1");

  if (!cmd_run_reset(c)) {
    plog(ERR, "Could not build kmod");

    return false;
  }

  return true;
}

bool compile_kernel_module(cmd_t c[static 1], test_t *test) {
  plog(INFO, "current working directory: %s", cwd);

  str sources = {};
  da_foreach(const char *, src, &test->sources) {
    str_append_cstr(&sources, *src);
    for (; sources.count > 0 && sources.items[sources.count - 1] != '.';
         sources.count--)
      ;
    sources.count--;

    str_append_cstr(&sources, ".o ");
  }

  const char *mitigate_flag = "";
  if (test->mitigate) {
    mitigate_flag = "-DMITIGATE";
  }
  const char *makefile_cont =
      tsprintf("ccflags-y += -I%s/%s  -D%s=1 -D%s %s\n"
               "obj-m += %s.o\n"
               "%s-objs := " str_fmt,
               cwd, include_dir_name, target_t_strs[test->opts.target],
               runner_t_strs[test->opts.runner], mitigate_flag,
               test->module_name, test->module_name, str_arg(&sources));

  da_free(&sources);

  return compile_kmod(
      c, makefile_cont,
      tsprintf("M=%s/%s/%s", cwd, module_dir, test->module_name));
}

bool compile_simulation_module(cmd_t c[static 1], test_t *test) {
  const char *test_dir =
      tsprintf("%s/tests", test->opts.extra_sim_options.chipyard.directory);
  const char *out_file = tsprintf("%s/test.c", test_dir);
  const char *out_args = tsprintf("%s/args.h.in", test_dir);

  // TODO: Mitigate flag in the SIM

  str t = {0};
  str_append_cstr(&t, "const void **args = 0;\n");
  // Write to a temp file that just needs to satisfy importing
  da_append(&t, '\0');
  if (access(out_args, F_OK) != 0 && !write_to_file(out_args, t.items)) {
    plog(ERR, "Faild to write %s: %s", out_file, strerror(errno));
    return KO;
  }
  t.count = 0;

  str_append_cstr(&t, tsprintf("#include \"%s/%s/%s/%s.c\"\n", cwd, module_dir,
                               test->module_name, test->test_file));

  str_append_cstr(&t, "#include \"args.h.in\"\n");

  da_append(&t, '\0');

  if (!write_to_file(out_file, t.items)) {
    plog(ERR, "Faild to write %s: %s", out_file, strerror(errno));
    return KO;
  }
  da_free(&t);

  cmd_append(c, "/bin/sh", "-c",
             tconcat(test->opts.extra_sim_options.shell, " -c ", "\"source ",
                     tsprintf("%s/env.sh",
                              test->opts.extra_sim_options.chipyard.directory),
                     " && make -C ", test_dir, " test\"", NULL));

  if (!cmd_run_reset(c)) {
    return false;
  }

  return true;
}

result_code_t run_user_test_as_shlib(cmd_t *c, test_t *t,
                                     struct run_function_request req,
                                     manager_t m) {
  (void)c;
  const usize s = tsave();
  const char *so = tsprintf("./%s.so", t->module_name);

  void *shlib = dlopen(so, RTLD_LAZY);
  if (!shlib) {
    plog(ERR, "Failed to open the shared library %s\n", dlerror());

    return KO;
  }

  long (*tester)(u32, struct run_function_request *) =
      get_func(shlib, "tester_run");

  cpu_set_t mask;
  CPU_ZERO(&mask);
  CPU_SET(t->opts.cpu, &mask);

  if (sched_setaffinity(0, sizeof(mask), &mask) == -1) {
    plog(ERR, "Can't set cpu %d: %s", t->opts.cpu, strerror(errno));
    return KO;
  }
  tester(RUN_FUNCTION, &req);

  t->result = req.ret;
  t->result_code = m.get_result_diagnostics(req.ret);
  trestore(s);

  dlclose(shlib);

  return t->result_code;
}

result_code_t run_user_test_as_exe(cmd_t *c, test_t *t,
                                   struct run_function_request req,
                                   manager_t m) {
  str in = {0};
  serialize_args(&in, req);

  if (!write_to_file_bin("data.in", (u8 *)in.items, in.count)) {
    plog(ERR, "Failed to save `data.in` for exe");
    return KO;
  }

  cmd_append(c, tsprintf("./%s", t->module_name), "data.in");
  if (!cmd_run_async(c, .fdout = NEW_READ_PIPE)) {
    plog(ERR, "Failed to run exe tester for %s", t->module_name);
    return false;
  }

  str cmd_out = {0};
  read_until_close(c->fdout, &cmd_out);

  const strv parsed = parse_between_delim((u8 *)cmd_out.items, cmd_out.count,
                                          DELIM, strlen(DELIM));

  memcpy(req.ret, parsed.items, parsed.count);
  t->result = req.ret;
  t->result_code = m.get_result_diagnostics(req.ret);

  return t->result_code;
}

result_code_t run_user_test(cmd_t *c, test_t *t,
                            struct run_function_request req, manager_t m) {
  if (t->opts.run_as_exe) {
    return run_user_test_as_exe(c, t, req, m);
  } else {
    return run_user_test_as_shlib(c, t, req, m);
  }
}

bool load_kernel_module(cmd_t c[static 1], test_t *t) {
  const char *module_ko = tsprintf("%s.ko", t->module_name);

  cmd_append(c, "insmod", module_ko);
  if (!cmd_run_reset(c)) {
    plog(ERR, "Could not run insmod %s", t->module_name);
    return false;
  }

  return true;
}

bool unload_kernel_module(cmd_t c[static 1], test_t *t) {
  const char *module_ko = tsprintf("%s.ko", t->module_name);

  cmd_append(c, "rmmod", module_ko);
  if (!cmd_run_reset(c)) {
    plog(ERR, "Failed to run rmmod for %s", t->module_name);
    return false;
  }

  return true;
}

result_code_t run_kernel_test(cmd_t *c, test_t *t,
                              struct run_function_request req, manager_t a) {

  if (!load_kernel_module(c, t))
    return KO;

  int fd;
  fd = open(tsprintf("/dev/tester_%s_device", t->module_name), O_RDWR);
  if (fd < 0) {
    printf("Failed to open device");
    goto remove_kmod;
  }

  int ret = ioctl(fd, RUN_FUNCTION, &req);
  if (ret < 0) {
    perror("Failed to open ioclt");
    goto close_fd;
  }

  t->result = req.ret;
  t->result_code = a.get_result_diagnostics(req.ret);

  close(fd);

  if (!unload_kernel_module(c, t))
    return KO;

  return t->result_code;

close_fd:
  close(fd);

remove_kmod:
  unload_kernel_module(c, t);
  cmd_run_reset(c);

  return KO;
}

#define serialize_field_aligned(sink, field)                                   \
  da_append_many(sink, (u8 *)&field, (8));

void serialize_args(str *out, struct run_function_request req) {
  serialize_field_aligned(out, req.args_count);
  serialize_field_aligned(out, req.cpu);
  for (int i = 0; i < req.args_count; i++) {
    serialize_field_aligned(out, req.args_sizes[i]);
    da_append_many(out, req.args[i], req.args_sizes[i]);
  }
}

void args_to_c_array(str *out, struct run_function_request req) {
  usize check = tsave();
  str args = {0};
  serialize_args(&args, req);

  str_append_cstr(
      out,
      tsprintf("const char __attribute__((aligned(8))) __args[%zu] = {\n    ",
               args.count));

  for (size_t i = 0; i < args.count; i++) {
    str_append_cstr(out, tsprintf("0x%02x", (u8)args.items[i]));

    if (i + 1 != args.count)
      str_append_cstr(out, ", ");

    if ((i + 1) % 12 == 0)
      str_append_cstr(out, "\n    ");
  }

  str_append_cstr(out, "\n};\n");
  trestore(check);
  da_free(&args);
}

void construct_args(str *out, struct run_function_request req) {
  usize check = tsave();
  args_to_c_array(out, req);
  str_append_cstr(out, tsprintf("const unsigned int cpu = %d;\n", req.cpu));

  str_append_cstr(out, tsprintf("void *args[] = {\n    "));
  usize idx = sizeof(req.args_count) + 8; // sizeof(req.cpu); // Misaligned read
  for (int i = 0; i < req.args_count; i++) {
    idx += sizeof(req.args_sizes);
    str_append_cstr(out, tsprintf("(void *)&__args[%d],\n", idx));
    idx += req.args_sizes[i];
  }
  str_append_cstr(out, "};\n");
  da_append(out, '\0');
  trestore(check);
}

strv parse_between_delim(u8 *buf, usize buflen, char *delim, usize delim_len) {
  strv parsed = {0};

  // find first delimiter
  u8 *start = memmem(buf, buflen, delim, delim_len);
  if (!start)
    return parsed;
  start += delim_len;

  // find second delimiter
  usize remaining = buflen - (start - buf);
  u8 *end = memmem(start, remaining, delim, delim_len);
  if (!end)
    return parsed;

  parsed.items = (char *)start;
  parsed.count = end - start;
  return parsed;
}

result_code_t run_simulation_test(cmd_t *c, test_t *t,
                                  struct run_function_request req,
                                  manager_t a) {
  usize check = tsave();

  str out = {0};

  construct_args(&out, req);

  const char *test_dir =
      tsprintf("%s/tests", t->opts.extra_sim_options.chipyard.directory);
  const char *out_args = tsprintf("%s/args.h.in", test_dir);
  if (!write_to_file(out_args, out.items)) {
    plog(ERR, "Faild to write %s: %s", out_args, strerror(errno));
    return KO;
  }
  da_free(&out);

  if (!compile_simulation_module(c, t)) {
    return KO;
  }

  assert(t->opts.sim_impl == SIM_CHIPYARD);
  const char *config = "CustomBoomV3Config";
  plog(INFO, "%d", c->c.count);
  // clang-format off
  cmd_append(c, "/bin/sh", "-c",
             tconcat(t->opts.extra_sim_options.shell,
                     " -c ", "\"source ",
                     tsprintf("%s/env.sh && ", t->opts.extra_sim_options.chipyard.directory),
                     tsprintf("cd %s && ", t->opts.extra_sim_options.chipyard.directory),
                     tsprintf("./sims/verilator/simulator-chipyard.harness-%s "
                              "+permissive "
                              "+dramsim "
                              "+dramsim_ini_dir=generators/testchipip/src/main/resources/dramsim2_ini "
                              "+fastloadmem "
                              "+loadmem=./tests/test.riscv "
                              "+permissive-off "
                              "./tests/test.riscv \""
                              , config)));
  // clang-format on

  /* cmd_run_reset(c); */
  /* exit(0); */
  if (!cmd_run_async(c, .fdout = NEW_READ_PIPE)) {
    plog(ERR, "Failed to run simulation for %s", t->module_name);
    return false;
  }

  str cmd_out = {0};
  read_until_close(c->fdout, &cmd_out);

  plog(INFO, str_fmt, str_arg(&cmd_out));

  const strv parsed = parse_between_delim((u8 *)cmd_out.items, cmd_out.count,
                                          DELIM, strlen(DELIM));

  memcpy(req.ret, parsed.items, parsed.count);
  t->result = req.ret;
  t->result_code = a.get_result_diagnostics(req.ret);

  trestore(check);
  return t->result_code;
}

result_code_t run_test(cmd_t *c, test_t *t, struct run_function_request r,
                       manager_t a) {
  static_assert(RUNNER_NUM == 3, "Update run test");

  static result_code_t (*const __run_test[RUNNER_NUM])(
      cmd_t *, test_t *, struct run_function_request, manager_t) = {
      [RUNNER_KERNEL] = run_kernel_test,
      [RUNNER_USER] = run_user_test,
      [RUNNER_SIMULATION] = run_simulation_test,
  };

  return __run_test[t->opts.runner](c, t, r, a);
}

bool compile_test(cmd_t c[static 1], test_t *t) {
  static_assert(RUNNER_NUM == 3, "Update compile test");

  static bool (*const __compile_module[RUNNER_NUM])(cmd_t *, test_t *) = {
      [RUNNER_KERNEL] = compile_kernel_module,
      [RUNNER_USER] = compile_user_module,
      [RUNNER_SIMULATION] = compile_simulation_module,
  };

  return __compile_module[t->opts.runner](c, t);
}

bool execute_dependency(cmd_t c[static 1], test_t *test) {
  if (chdir(test->module_path) != 0) {
    plog(ERR, "Failed to change directory: %p", test->module_path);
    return false;
  }

  if (test->depends_on.count > 0ULL) {
    if (chdir(cwd)) {
      plog(ERR, "Failed to change directory: %p", cwd);
      return false;
    };

    if (!execute_dependencies(test)) {
      // When mitigating the features we still want to test stuff, so we can't
      // stop just because a test is failing
      if (!test->mitigate) {
        return false;
      }
    }

    if (chdir(test->module_path) != 0) {
      plog(ERR, "Failed to change directory: %p", test->module_path);
      return false;
    }
  }

  const char *test_define_name =
      tsprintf(test_define_name_templ, test->module_name, test->module_name);
  if (!write_to_file("test_name.h.out", test_define_name)) {
    plog(ERR, "Failed to write file: %s", strerror(errno));
    return false;
  }

  u64 *local_clock = malloc(sizeof(u64));
  *local_clock = test->opts.clock_speed;

  size_t total_args = test->depends_on.count + 1;
  request_dependencies_t *args = malloc(sizeof(*args) * total_args);
  usize *args_sizes = malloc(sizeof(*args_sizes) * total_args);
  args[0] = (request_dependencies_t)local_clock;
  args_sizes[0] = sizeof(u64);

  bool dep_failed = false;
  for (size_t i = 0; i < test->depends_on.count; i++) {
    test_t *dep = test_find(test->depends_on.items[i], test->opts.target,
                            test->opts.runner, test->opts.cpu);
    expect(dep != NULL);

    args[i + 1] = dep->result;
    args_sizes[i + 1] = dep->result_size;

    // In case we try to run something that has a dependecy that has already
    // failed, fail immediately
    if (dep->result_code != OK && !dep->mitigate) {
      dep_failed = true;
    }
  }

  usize tmp_save = tsave();

  manager_t manager = {0};
  volatile result_code_t r = RETRY;
  do {
    if (!get_manager(&manager, test)) {
      trestore(tmp_save);
      return false;
    }

    r = manager.setup(args);
    if (r == RETRY) {
      dlclose(manager.shlib);
    }

  } while (r == RETRY);

  if (r == KO) {
    trestore(tmp_save);
    return false;
  }

  trestore(tmp_save);

  test->result_size = manager.get_result_size();
  test->result = malloc(test->result_size);
  memset(test->result, 0, test->result_size);

  // Make sure we still allocate stuff to not crash, and exit early BUT
  // When mitigating the features we still want to test stuff, so we can't
  // stop just because a test is failing
  if (dep_failed && !test->mitigate) {
    test->result_code = KO;
    goto exit;
  }
  struct run_function_request req = {
      .args_count = total_args,
      .args = args,
      .args_sizes = args_sizes,
      .cpu = test->opts.cpu,
      .ret = test->result,
  };

  plog(INFO, "Begin execution for %s", test->module_name);
  do {
    if (!compile_test(c, test)) {
      plog(ERR, "Failed to compile the test... exiting");
      goto exit;
    }

    test->result_code = run_test(c, test, req, manager);
  } while (test->result_code == RETRY);

exit:
  free(args[0]);
  free(args);
  free(args_sizes);
  cmd_reset(c);

  if (chdir(cwd)) {
    plog(ERR, "Failed to change directory: %p", cwd);
    return false;
  };

  dlclose(manager.shlib);
  return test->result_code == OK;
}

bool execute_dependencies(test_t *parent) {
  cmd_t cmd = {0};
  usize check = 0;

  for (usize i = 0; i < parent->depends_on.count; i++) {
    test_t *t = test_find(parent->depends_on.items[i], parent->opts.target,
                          parent->opts.runner, parent->opts.cpu);

    if (t == NULL) {
      t = test_new(parent->depends_on.items[i], parent->opts);

      check = tsave();
      if (t == NULL)
        goto fail;

      // Don't care if it actually failed, make the full graph
      execute_dependency(&cmd, t);

      trestore(check);
    }
  }

  cmd_free(&cmd);
  return true;

fail:

  trestore(check);
  cmd_free(&cmd);
  return false;
}

static void segfault_handler(int sig, siginfo_t *info, void *ucontext) {
  plog(ERR, "Detected memory fault");

  cmd_t cmd = {};

  cmd_append(&cmd, "rmmod", "probe");
  cmd_run_reset(&cmd);

  _exit(1);
}

bool make_template(const char *new) {
#define TEMPLATE_DIR "modules/template/"
  const char *template_module = TEMPLATE_DIR "template_module.json.in";
  const char *template_test_c = TEMPLATE_DIR "template_module.c.in";
  const char *template_test_h = TEMPLATE_DIR "template_module.h.in";
  const char *template_manager_c = TEMPLATE_DIR "template_manager.c.in";
  const char *gitignore = TEMPLATE_DIR ".gitignore";

  str buf = {0};
  const char *to_write;
  if (mkdir(tsprintf("modules/%s", new), 0755) != 0) {
    plog(ERR, "Could create directory modules/%s because %s", new,
         strerror(errno));
  }

  {
    const char *curr = template_module;
    if (!read_file(curr, &buf)) {
      plog(ERR, "Could not read file %s because %s", curr, strerror(errno));
    }
    to_write = tsprintf(buf.items, new);
    if (!write_to_file(tsprintf("modules/%s/module.json", new), to_write)) {
      plog(ERR, "Could not writefile %s because %s", curr, strerror(errno));
    }
    buf.count = 0;
  }
  {
    const char *curr = template_test_c;
    if (!read_file(curr, &buf)) {
      plog(ERR, "Could not read file %s because %s", curr, strerror(errno));
    }
    to_write = tsprintf(buf.items, new, new);
    if (!write_to_file(tsprintf("modules/%s/%s_test.c", new, new), to_write)) {
      plog(ERR, "Could not writefile %s because %s", curr, strerror(errno));
    }
    buf.count = 0;
  }
  {
    const char *curr = template_test_h;
    if (!read_file(curr, &buf)) {
      plog(ERR, "Could not read file %s because %s", curr, strerror(errno));
    }
    to_write = tsprintf(buf.items, new, new, new);
    if (!write_to_file(tsprintf("modules/%s/%s_test.h", new, new), to_write)) {
      plog(ERR, "Could not writefile %s because %s", curr, strerror(errno));
    }
    buf.count = 0;
  }
  {
    const char *curr = template_manager_c;
    if (!read_file(curr, &buf)) {
      plog(ERR, "Could not read file %s because %s", curr, strerror(errno));
    }
    to_write = tsprintf(buf.items, new, new, new, new);
    if (!write_to_file(tsprintf("modules/%s/%s_manager.c", new, new),
                       to_write)) {
      plog(ERR, "Could not writefile %s because %s", curr, strerror(errno));
    }
    buf.count = 0;
  }
  {
    const char *curr = gitignore;
    if (!read_file(curr, &buf)) {
      plog(ERR, "Could not read file %s because %s", curr, strerror(errno));
    }
    to_write = tsprintf(buf.items);
    if (!write_to_file(tsprintf("modules/%s/.gitignore", new), to_write)) {
      plog(ERR, "Could not writefile %s because %s", curr, strerror(errno));
    }
    buf.count = 0;
  }

  return true;
}

void print_help(const char *program_name, int exit_code) {
  str targets = {0};
  for (int i = 0; i < TARGET_NUM; i++) {
    str_append_cstr(&targets, target_t_strs[i]);
    if (i < TARGET_NUM - 1) {
      str_append_cstr(&targets, ", ");
    }
  }

  str runners = {0};
  for (int i = 0; i < RUNNER_NUM; i++) {
    str_append_cstr(&runners, runner_t_strs[i]);
    if (i < RUNNER_NUM - 1) {
      str_append_cstr(&runners, ", ");
    }
  }

  printf("help %s:\n"
         "\t--new/-n\t\tCreates a new module with name <arg>\n"
         "\t--target/-t\t\tGive the architeture to compile to (" str_fmt ")\n"
         "\t--runner/-r\t\tRunner for the test (" str_fmt ")\n"
         "\t--clock-speed/-c\t\tClock speed of the CPU\n"
         "\t--kernel-headers/-k\t\tKernel headers directory\n"
         "\t--mitigate/-m\t\tRemove feature detection\n"
         "\t--save/-s\t\tSave run\n"
         "\t--help/-h\t\tPrint this help\n",
         program_name, str_arg(&targets), str_arg(&runners));
  exit(exit_code);
}

void print_help_sims(const char *program_name, int exit_code) {
  str sims = {0};
  for (int i = 0; i < SIM_NUM; i++) {
    str_append_cstr(&sims, simulation_impl_t_strs[i]);
    if (i < SIM_NUM - 1) {
      str_append_cstr(&sims, ", ");
    }
  }

  printf("help %s --runner RUNNER_SIMULATION:\n"
         "\t--platform/-p\t\tType of simulation platform (" str_fmt ")\n"
         "\t--shell/-s\t\tShell to be used when using runner SIMULATION\n"
         "\t--help/-h\t\tPrint this help\n\n",
         program_name, str_arg(&sims));

  print_help(program_name, exit_code);
}

void print_help_sims_chipyard(const char *program_name, int exit_code) {
  printf("help %s --runner RUNNER_SIMULATION --platform SIM_CHIPYARD:\n"
         "\t--directory/-d\t\tHome of the chipyard installation\n"
         "\t--help/-h\t\tPrint this help\n\n",
         program_name);

  print_help_sims(program_name, exit_code);
}

int parse_sim_chipyard_options(const char *program_name, int argc, char *argv[],
                               run_options_t *opts) {

  int saved_optind = optind;
  optind = 0;

  if (argc <= 0) {
    plog(0, "Not enough arguments for simulation");
    print_help_sims_chipyard(program_name, 1);
  }

  int opt;
  while ((opt = getopt_long(
              argc, argv, "+d:h",
              (struct option[]){{"directory", required_argument, 0, 'd'},
                                {"help", no_argument, 0, 'h'},
                                {0, 0, 0, 0}},
              NULL)) != -1) {
    switch (opt) {

    case 'd':
      opts->extra_sim_options.chipyard.directory = strdup(optarg);
      break;

    case 'h':
      print_help_sims_chipyard(program_name, 0);
      break;

    case '?':
      optind -= 1;
      goto exit;

    default:
      plog(INFO, "Unrecognized flag: %c", opt);
      print_help_sims_chipyard(program_name, 1);
    }
  next_option:;
  }
exit:

  int to_ret = optind;
  optind = saved_optind;
  return to_ret - 1;
}

int (*sim_opts_parsers[SIM_NUM])(const char *, int, char *[],
                                 run_options_t *) = {
    [SIM_CHIPYARD] = parse_sim_chipyard_options,
};

int parse_sim_options(const char *program_name, int argc, char *argv[],
                      run_options_t *opts) {
  int saved_optind = optind;
  optind = 1;

  if (argc <= 0) {
    plog(0, "Not enough arguments for simulation");
    print_help_sims(program_name, 1);
  }

  int opt;
  while ((opt = getopt_long(
              argc, argv, "+p:s:h",
              (struct option[]){{"platform", required_argument, 0, 'p'},
                                {"shell", required_argument, 0, 'c'},
                                {"help", no_argument, 0, 'h'},
                                {0, 0, 0, 0}},
              NULL)) != -1) {
    switch (opt) {
    case 'p':
      for (int i = 0; i < SIM_NUM; i++) {
        if (strcmp(simulation_impl_t_strs[i], optarg) == 0) {
          opts->sim_impl = (simulation_impl_t)i;
          optind += sim_opts_parsers[opts->sim_impl](
              program_name, argc - (optind - 1), argv + (optind - 1), opts);

          /* plog(INFO, "++%s", argv[optind]); */
          /* plog(INFO, "++%d", optind); */
          goto next_option;
        }
      }
      plog(INFO, "Unrecognized simulation implementation: %s (CASE SENSITIVE)",
           optarg);
      print_help_sims(program_name, 1);
      break;

    case 's':
      opts->extra_sim_options.shell = strdup(optarg);
      break;

    case 'h':
      print_help_sims(program_name, 0);
      break;

    case '?':
      optind -= 1;
      goto exit;

    default:
      /* break; */
      plog(INFO, "Unrecognized flag: %c %d", opt, opt);
      print_help_sims(program_name, 1);
    }
  next_option:;
  }

exit:
  if (opts->extra_sim_options.shell == NULL) {
    plog(INFO, "Missing shell");
    print_help_sims(program_name, 1);
  }

  int to_ret = optind;
  optind = saved_optind;
  return to_ret - 1;
}

void parse_options(int argc, char *argv[], run_options_t *opts) {
  const char *program_name = argv[0];
  opts->target = (u32)-1;
  opts->runner = (u32)-1;
  opterr = 0;
  opts->to_mitigate = "";

  int opt;
  while ((opt = getopt_long(
              argc, argv, "+n:t:r:c:hm:s::k:",
              (struct option[]){{"new", required_argument, 0, 'n'},
                                {"target", required_argument, 0, 't'},
                                {"runner", required_argument, 0, 'r'},
                                {"clock-speed", required_argument, 0, 'c'},
                                {"kernel-headers", required_argument, 0, 'k'},
                                {"mitigate", required_argument, 0, 'm'},
                                {"save", optional_argument, 0, 's'},
                                {"help", no_argument, 0, 'h'},
                                {0, 0, 0, 0}},
              NULL)) != -1) {
    /* printf("++ %s\n", argv[optind]); */
    switch (opt) {
    case 'n':
      make_template(optarg);
      exit(0);

    case 't':
      for (int i = 0; i < TARGET_NUM; i++) {
        if (strcmp(target_t_strs[i], optarg) == 0) {
          opts->target = (target_t)i;
          goto next_option_main;
        }
      }
      plog(INFO, "Unrecognized target: %s (CASE SENSITIVE)", optarg);
      print_help(program_name, 1);
      break;

    case 'm':
      opts->to_mitigate = strdup(optarg);
      break;

    case 's':
      printf("%p\n", optarg);
      if (optarg) {
        opts->save_file_name = strdup(optarg);
      }
      opts->save = true;
      break;

    case 'r':
      for (int i = 0; i < RUNNER_NUM; i++) {
        if (strcmp(runner_t_strs[i], optarg) == 0) {
          opts->runner = (runner_t)i;
          if (opts->runner == RUNNER_SIMULATION) {
            optind += parse_sim_options(program_name, argc - (optind - 1),
                                        argv + (optind - 1), opts);
          }

          /* plog(INFO, "---- %s", argv[optind]); */
          /* plog(INFO, "---- %d", optind); */
          goto next_option_main;
        }
      }
      plog(INFO, "Unrecognized runner: %s (CASE SENSITIVE)", optarg);
      print_help(program_name, 1);
      break;

    case 'c':
      opts->clock_speed = strtoul(optarg, NULL, 10);
      break;

    case 'k':
      kernel_header_dir = strdup(optarg);
      break;

    case 'h':
      print_help(program_name, 0);
      break;

    case '?':
      /* printf("?? %s\n", argv[optind]); */
      optind -= 1;
      break;

    default:
      plog(0, "Unrecognized flag: %c", opt);
      print_help(program_name, 1);
    }
  next_option_main:;
  }

  if (opts->target == (u32)-1 || opts->runner == (u32)-1) {
    plog(ERR, "--target and --runner are required.");
    print_help(program_name, 1);
  }

  if (opts->clock_speed == 0) {
    plog(ERR, "Clock speed is required");
    print_help(program_name, 1);
  }

  /* plog(INFO, "---- %s", argv[optind - 1]); */
  /* plog(INFO, "---- %d", optind); */
  /* optind -= 1; */
}

int main(int argc, char *argv[]) {
  const char *program_name = argv[0];

  {
    struct rlimit rl = {
        .rlim_cur = 64 * 1024 * 1024,
        .rlim_max = 64 * 1024 * 1024,
    };
    if (setrlimit(RLIMIT_STACK, &rl) != 0) {
      perror("setrlimit(RLIMIT_STACK)");
    }
  }

  run_options_t opts = {
      .target = -1,
      .runner = -1,
      .sim_impl = -1,
  };

  parse_options(argc, argv, &opts);
  /* plog(INFO, opts.extra_sim_options.shell); */

  if (optind >= argc) {
    plog(ERR, "missing module name");
    return 1;
  }

  const char *module = argv[optind];
  plog(INFO, "module name: %s", module);

  cwd = getcwd(cwd, 0);
  if (cwd == NULL) {
    plog(ERR, "failed to get cwd");
    return 1;
  }

  if (kernel_header_dir == NULL) {
    const char *locations[] = {"/usr/src/kernels", "/usr/src", NULL};
    bool found = false;

    for (int loc = 0; locations[loc] != NULL; loc++) {
      paths_t paths = {0};
      if (!read_dir(locations[loc], &paths))
        continue;

      paths_t valid = {0};
      for (int i = 0; i < (int)paths.count; i++) {
        const char *path = paths.items[i];
        if (path[0] == '.')
          continue;
        if (loc == 1 && strncmp(path, "linux-headers-", 14) != 0)
          continue;
        da_append(&valid, path);
      }

      if (valid.count > 0) {
        int choice = 0;
        if (valid.count > 1) {
          plog(INFO, "Multiple kernel headers found in %s:", locations[loc]);
          for (int i = 0; i < (int)valid.count; i++)
            plog(INFO, "  [%d] %s", i, valid.items[i]);
          printf("Select kernel headers [0-%d]: ", (int)valid.count - 1);
          char buf[64];
          if (fgets(buf, sizeof(buf), stdin))
            choice = atoi(buf);
          if (choice < 0 || choice >= (int)valid.count)
            choice = 0;
        }
        kernel_header_dir =
            tsprintf("%s/%s", locations[loc], valid.items[choice]);
        found = true;
      }

      da_free(&paths);
      da_free(&valid);
      if (found)
        break;
    }

    if (!found) {
      plog(ERR,
           "No valid kernel headers found in /usr/src/kernels or /usr/src");
      return 1;
    }
  }

  if (opts.runner == RUNNER_USER) {
    struct sched_param param;
    param.sched_priority = 99;

    if (sched_setscheduler(0, SCHED_RR, &param) == -1) {
      perror("sched_setscheduler");
      exit(EXIT_FAILURE);
    }
  }
  plog(INFO, "kernel headers used: %s", kernel_header_dir);
  plog(INFO, "%d", __tmpbuf_curr_size);

  u64 *clock_speed = malloc(sizeof(u64));
  *clock_speed = opts.clock_speed;
  test_t t = {
      .opts = opts,
      .module_name = "root",
      .result_code = OK,
      .result_size = sizeof(u64),
      .result = clock_speed,
  };
  da_append(&runned_test, t);

  if (strcmp(module, "all") == 0) {
    char **modules;
    s32 n = find_modules("./modules", &modules);
    running_all = true;

    if (n < 0) {
      plog(ERR, "No modules found in `./modules` dir");
      exit(1);
    }

    plog(INFO, "Scheduled tests:");
    for (s32 i = 0; i < n; i++) {
      da_append(&t.depends_on, modules[i]);
      plog(INFO, "\t- %s", modules[i]);
    }

  } else {
    da_append(&t.depends_on, module);
  }

  cmd_t cmd = {0};
  test_t probe = {
      .opts =
          {
              .runner = RUNNER_KERNEL,
              .target = opts.target,
          },
      .module_name = "probe",
  };
  int ret = EXIT_SUCCESS;

  if (opts.runner == RUNNER_USER) {
    if (chdir("probe/") != 0) {
      plog(ERR, "Failed to change directory: probe");
      goto exit;
    }

    const char *makefile_cont =
        tsprintf("ccflags-y += -I%s/%s  -D%s=1 -D%s\n"
                 "obj-m += probe.o\n"
                 "probe-objs := probe_mod.o",
                 cwd, include_dir_name, target_t_strs[opts.target],
                 runner_t_strs[RUNNER_KERNEL]);

    if (!compile_kmod(&cmd, makefile_cont, tsprintf("M=%s/probe", cwd))) {
      ret = EXIT_FAILURE;
      goto exit;
    }
    if (!load_kernel_module(&cmd, &probe)) {

      unload_kernel_module(&cmd, &probe);
      if (!load_kernel_module(&cmd, &probe)) {
        ret = EXIT_FAILURE;
        goto exit;
      }
    }
    if (chdir("..") != 0) {
      plog(ERR, "Failed to change directory: probe");
      goto exit;
    }

    struct sigaction sa;
    sa.sa_sigaction = segfault_handler;

    sa.sa_flags = SA_SIGINFO | SA_RESTART;

    /* sigaction(SIGSEGV, &sa, NULL); */
    sigaction(SIGINT, &sa, NULL);
  }

  /* load_run("./run.bin"); */

  if (!execute_dependencies(&t)) {
    plog(ERR, "Execution failed");
  }

  char timestamp[40] = {0};
  get_timestamp_utc(timestamp, 40);
  if (opts.save) {
    if (opts.save_file_name) {
      save_run(tsprintf("%s.bin", opts.save_file_name));
    } else {
      save_run(tsprintf("%s_%s.bin", module, timestamp));
    }
  }

  if (opts.runner == RUNNER_USER) {
    if (chdir("probe/") != 0) {
      plog(ERR, "Failed to change directory: probe");
      goto exit;
    }
    unload_kernel_module(&cmd, &probe);
    if (chdir("..") != 0) {
      plog(ERR, "Failed to change directory: %s", cwd);
      goto exit;
    }
  }
exit:
  da_foreach(test_t, t, &runned_test) { test_free(t); }
  da_free(&runned_test);
  da_free(&t.depends_on);

  return ret;
}
