#include <dlfcn.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define IMPLEMENTATIONS
#include "libs/lbstd.h"

#define JIMP_IMPLEMENTATION
#include "libs/jim/jimp.h"

// TODO: Find a better way to communicate the result back to the orchestrator
// such that we don't need to recompile
#include "src/commands.h"

#define DEFAULT_STR_MAX_SIZE 32

#define EACH_RUNNER(X)                                                         \
  X(RUNNER_USER)                                                               \
  X(RUNNER_KERNEL)                                                             \
  X(RUNNER_SIMULATION)                                                         \
  X(RUNNER_NUM)

typedef_enum(runner_t, EACH_RUNNER);

#define EACH_TARGET(X)                                                         \
  X(TARGET_X86_64)                                                             \
  X(TARGET_RISCV32)                                                            \
  X(TARGET_NUM)

typedef_enum(target_t, EACH_TARGET);

const char *module_dir = "modules";
const char *json = "module.json";
const char *module_name = "cache";
char *cwd;

typedef struct {
  const char *module_name;
  const char *test_file;
  da(const char *) sources;
  da(const char *) depends_on;

  const target_t target;
  const runner_t runner;

  bool passed;
  void *result;
} test_t;

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

  da_append(&out->sources, tsprintf("%s.c", out->test_file));

  return true;
}

const char *include_dir = "include/";

void make_kernel_module(cmd_t *c, test_t test, const str uname,
                        const char *cwd) {
  const char *makefile_path =
      tsprintf("%s/%s/Makefile", module_dir, test.module_name);

  str sources = {};
  da_foreach(const char *, src, &test.sources) {
    str_append_cstr(&sources, *src);
    for (; sources.count >= 0 && sources.items[sources.count - 1] != '.';
         sources.count--)
      ;
    sources.count--;

    str_append_cstr(&sources, ".o ");
  }

  const char *makefile_cont =
      tsprintf("ccflags-y += -I%s/%s  -D%s=1 -D%s\n"
               "obj-m += %s.o\n"
               "%s-objs := " str_fmt "../../%s/%s_immintr.o",
               cwd, include_dir, target_t_strs[test.target],
               runner_t_strs[test.runner], test.module_name, test.module_name,
               str_arg(&sources), include_dir, target_t_strs[test.target]);

  const char *in = tsprintf("%s/%s/%s", cwd, module_dir, test.test_file);
  const char *out = tsprintf("%s/%s/%s", cwd, module_dir, test.module_name);

  write_file(makefile_path, makefile_cont);

  cmd_append(c, __BEAR "make", "-C",
             tsprintf("/lib/modules/%.*s/build", uname.count - 1, uname.items),
             tsprintf("M=%s", out), "modules", "V=1");
}

typedef struct {
  u64 (*get_result_size)();
  bool (*get_result_diagnostics)(void *);
} analyzer_t;

const char *make_shared_lib(cmd_t *c, const char *working_dir, const char *name,
                            int n, const char *sources[static n]) {

  const char *out = tsprintf("%s/%s.so", working_dir, name);

  cmd_append(c, __BEAR "cc", "-ggdb", "-fPIC", "-shared",
             tsprintf("-I%s", include_dir), "-o", out);

  for (int i = 0; i < n; i++) {
    cmd_append(c, tsprintf("%s/%s", working_dir, sources[i]));
  }

  return out;
}

bool get_analyzer(analyzer_t out[static 1], test_t t[static 1]) {
  const char *working_dir = tsprintf("%s/%s", module_dir, t->module_name);
  const char *in = tsprintf("%s_analyze.c", t->module_name);

  cmd_t c = {0};
  const char *so =
      make_shared_lib(&c, working_dir, in, 1, (const char *[]){in});

  if (!cmd_run_reset(&c)) {
    plog(ERR, "Failed to compile the shared library %s: %s\n", in,
         strerror(errno));

    return false;
  }

  cmd_free(&c);

  void *shlib = dlopen(so, RTLD_LAZY);
  if (!shlib) {
    plog(ERR, "Failed to open the shared library %s: %s\n", dlerror());

    return false;
  }

  char *diag_func = tsprintf("%s_result_diagnostics", t->module_name);
  void *diag = dlsym(shlib, diag_func);
  if (!diag) {
    plog(ERR, "Failed to get diagnostic function %s: %s\n", diag_func,
         dlerror());

    return false;
  }

  char *size_func = tsprintf("%s_result_size", t->module_name);
  void *size = dlsym(shlib, size_func);
  if (!size) {
    plog(ERR, "Failed to get diagnostic function %s: %s\n", size_func,
         dlerror());

    return false;
  }

  out->get_result_size = size;
  out->get_result_diagnostics = diag;

  return true;
}

bool get_config_for_module(test_t out[static 1]) {
  Jimp jimp = {0};

  char *config_path = tsprintf("%s/%s/%s", module_dir, module_name, json);

  str file = {0};
  read_file(config_path, &file);

  jimp_begin(&jimp, config_path, file.items, file.count);

  if (!parse_test(&jimp, out)) {
    plog(ERR, "could not read the config file for moodule %s", module_name);

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

bool compile_user_module(cmd_t c[static 1], test_t test) {
  const char *working_dir = tsprintf("%s/%s", module_dir, test.module_name);

  make_shared_lib(c, working_dir, test.module_name, test.sources.count,
                  test.sources.items);

  cmd_append(
      c, tsprintf("%s/%s_immintr.S", include_dir, target_t_strs[test.target]));
  cmd_append(c, tsprintf("-D%s", target_t_strs[test.target]),
             tsprintf("-D%s", runner_t_strs[test.runner]));

  if (!cmd_run_reset(c)) {
    plog(ERR, "could not compile user module");
    return false;
  }

  return true;
}

bool compile_kernel_module(cmd_t c[static 1], test_t test) {
  cmd_append(c, "uname", "-r");
  if (!cmd_run(c, .fdout = NEW_READ_PIPE)) {
    plog(ERR, "Could not read uname");
    return false;
  }

  str uname = {0};
  read_fd(c->fdout, &uname);
  plog(INFO, "Uname -r: %s", uname.items);
  cmd_reset(c);

  plog(INFO, "current working directory: %s", cwd);
  make_kernel_module(c, test, uname, cwd);

  if (!cmd_run_reset(c)) {
    plog(ERR, "Could not build kernel module");

    da_free(&uname);
    return false;
  }

  da_free(&uname);
  return true;
}

bool run_user_test(cmd_t *c, test_t *t, struct run_function_request req,
                   analyzer_t a) {
  const char *so =
      tsprintf("%s/%s/%s.so", module_dir, t->module_name, t->module_name);

  void *shlib = dlopen(so, RTLD_LAZY);
  if (!shlib) {
    plog(ERR, "Failed to open the shared library %s: %s\n", dlerror());

    return false;
  }

  char *tester_func = "tester_run";
  void *tester = dlsym(shlib, "tester_run");
  if (!tester) {
    plog(ERR, "Failed to get diagnostic function %s: %s\n", tester_func,
         dlerror());

    dlclose(shlib);
    return false;
  }

  ((long (*)(u32, struct run_function_request *))tester)(RUN_FUNCTION, &req);

  t->result = req.ret;
  t->passed = a.get_result_diagnostics(req.ret);

  dlclose(shlib);

  return true;
}

bool run_kernel_test(cmd_t *c, test_t *t, struct run_function_request req,
                     analyzer_t a) {

  const char *module_ko =
      tsprintf("%s/%s/%s.ko", module_dir, t->module_name, t->module_name);

  cmd_append(c, "insmod", module_ko);
  if (!cmd_run_reset(c)) {
    plog(ERR, "Could not run insmod");
  }

  int fd;

  fd = open(tsprintf("/dev/tester_%s_device", t->module_name), O_RDWR);
  if (fd < 0) {
    printf("Failed to open device");
    goto remove_kmod;
  }

  int ret = ioctl(fd, RUN_FUNCTION, &req);
  if (ret < 0) {
    perror("Failed to read CR registers");
    goto close_fd;
  }

  t->result = req.ret;
  t->passed = a.get_result_diagnostics(req.ret);

  close(fd);

  cmd_append(c, "rmmod", module_ko);
  cmd_run_reset(c);
  return true;

close_fd:
  close(fd);

remove_kmod:
  cmd_append(c, "rmmod", module_ko);
  cmd_run_reset(c);

  return false;
}

bool run_test(cmd_t *c, test_t *t, struct run_function_request r,
              analyzer_t a) {
  static_assert(RUNNER_NUM == 3, "Update compile test");

  static bool (*const __run_test[RUNNER_NUM])(
      cmd_t *, test_t *, struct run_function_request, analyzer_t) = {
      [RUNNER_KERNEL] = run_kernel_test,
      [RUNNER_USER] = run_user_test,
      [RUNNER_SIMULATION] = 0,
  };

  return __run_test[t->runner](c, t, r, a);
}

bool compile_test(cmd_t *c, test_t t) {
  static_assert(RUNNER_NUM == 3, "Update compile test");

  static bool (*const __compile_module[RUNNER_NUM])(cmd_t *, test_t) = {
      [RUNNER_KERNEL] = compile_kernel_module,
      [RUNNER_USER] = compile_user_module,
      [RUNNER_SIMULATION] = 0,
  };

  return __compile_module[t.runner](c, t);
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
         "\t--target/-t\t\tGive the architeture to compile to (" str_fmt ")\n"
         "\t--runner/-r\t\tRunner for the test (" str_fmt ")\n"
         "\t--help/-h\t\tPrint this help\n",
         program_name, str_arg(&targets), str_arg(&runners));
  exit(exit_code);
}

int main(int argc, char *argv[], char *envp[]) {
  const char *program_name = argv[0];

  int opt;

  target_t target = -1;
  runner_t runner = -1;

loop:
  while (1) {
    int option_index = 0;
    static struct option long_options[] = {
        {"target", required_argument, 0, 't'},
        {"runner", required_argument, 0, 'r'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}};

    opt = getopt_long(argc, argv, "t:r:h", long_options, &option_index);
    if (opt == -1)
      break;

    switch (opt) {
    case 't':
      for (int i = 0; i < TARGET_NUM; i++) {
        if (memcmp(target_t_strs[i], optarg, strlen(target_t_strs[i])) == 0) {
          target = (target_t)i;
          goto loop;
        }
      }

      plog(ERR, "unrecognized target: %s (CASE SENSITIVE)", optarg);
      print_help(program_name, 1);

      break;

    case 'r':
      for (int i = 0; i < RUNNER_NUM; i++) {
        if (memcmp(runner_t_strs[i], optarg, strlen(runner_t_strs[i])) == 0) {
          runner = (runner_t)i;
          goto loop;
        }
      }

      plog(ERR, "unrecognized runner: %s (CASE SENSITIVE)", optarg);
      print_help(program_name, 1);
      break;

    case 'h':
      print_help(program_name, 0);
      break;

    default:
      plog(ERR, "unrecognized flag: %c", opt);
      print_help(program_name, 0);
    }
  }

  if (target == -1 || runner == -1) {
    plog(ERR, "Error: --target and --runner are required.\n");
    print_help(program_name, 1);
  }

  if (optind >= argc) {
    plog(ERR, "missing module name");
  }
  const char *module = argv[optind];

  test_t t = {
      .runner = runner,
      .target = target,
      .module_name = module,
  };

  cwd = getcwd(cwd, 0);
  if (cwd == NULL) {
    plog(ERR, "failed to get cwd");
    return 1;
  }

  if (!get_config_for_module(&t)) {
    return 1;
  }

  cmd_t c = {};
  const char *test_name_define_h =
      tsprintf("%s/%s/test_name.h.out", module_dir, t.module_name);

  write_file(test_name_define_h,
             tsprintf(test_define_name_templ, t.module_name, t.module_name));

  analyzer_t cache_analyzer = {0};
  if (!get_analyzer(&cache_analyzer, &t)) {
    return 1;
  }

  compile_test(&c, t);

  u64 result_size = cache_analyzer.get_result_size();
  void *result = malloc(result_size);

  struct run_function_request r = {
      .args = NULL,
      .cpu = 5,
      .ret = result,
  };

  run_test(&c, &t, r, cache_analyzer);

  da_free(&c.c);
  free(result);
  free(cwd);
  return 0;
}
