#ifndef _LBSTD
#define _LBSTD

#include <argp.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// ---------------------------------------------------------
// Usefull macros to make other macros
// ---------------------------------------------------------
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

// ---------------------------------------------------------
// Utilities
// ---------------------------------------------------------
#define expect(cond) assert(__FILE__ TOSTRING(__LINE__) && cond)
#define unreachable(msg) expect(msg && false)
#define todo(s) expect(s && false);
#define maybe(s) s

#define enum_gen_field(e) e,
#define enum_str_gen_field(e) [e] = #e,

#define typedef_enum(name, each_elem)                                          \
  typedef enum { each_elem(enum_gen_field) } name;                             \
  const char *name##_strs[] = {each_elem(enum_str_gen_field)};

#define shift(xs, xs_sz) (expect((xs_sz) > 0), (xs_sz)--, *(xs)++)

// ---------------------------------------------------------
// Dynamic Arrays
// ---------------------------------------------------------
#define da(T)                                                                  \
  struct {                                                                     \
    T *items;                                                                  \
    size_t count;                                                              \
    size_t capacity;                                                           \
  }

// --- from nob.h
#ifndef DA_INIT_CAP
#define DA_INIT_CAP 256
#endif

#define da_reserve(da, expected_capacity)                                      \
  do {                                                                         \
    if ((expected_capacity) > (da)->capacity) {                                \
      if ((da)->capacity == 0) {                                               \
        (da)->capacity = DA_INIT_CAP;                                          \
      }                                                                        \
      while ((expected_capacity) > (da)->capacity) {                           \
        (da)->capacity *= 2;                                                   \
      }                                                                        \
      (da)->items =                                                            \
          realloc((da)->items, (da)->capacity * sizeof(*(da)->items));         \
      expect((da)->items != NULL && "ERROR: Out of memory");                   \
    }                                                                          \
  } while (0)

// Append an item to a dynamic array
#define da_append(da, item)                                                    \
  do {                                                                         \
    da_reserve((da), (da)->count + 1);                                         \
    (da)->items[(da)->count++] = (item);                                       \
  } while (0)

// Append several items to a dynamic array
#define da_append_many(da, new_items, new_items_count)                         \
  do {                                                                         \
    da_reserve((da), (da)->count + (new_items_count));                         \
    memcpy((da)->items + (da)->count, (new_items),                             \
           (new_items_count) * sizeof(*(da)->items));                          \
    (da)->count += (new_items_count);                                          \
  } while (0)

#define da_resize(da, new_size)                                                \
  do {                                                                         \
    da_reserve((da), new_size);                                                \
    (da)->count = (new_size);                                                  \
  } while (0)

#define da_last(da) (da)->items[(expect((da)->count > 0), (da)->count - 1)]
#define da_remove_unordered(da, i)                                             \
  do {                                                                         \
    size_t j = (i);                                                            \
    expect(j < (da)->count);                                                   \
    (da)->items[j] = (da)->items[--(da)->count];                               \
  } while (0)

#define da_foreach(Type, it, da)                                               \
  for (Type *it = (da)->items; it < (da)->items + (da)->count; ++it)

#define da_free(da) free((da)->items)

// end nob.h
#define view(T)                                                                \
  struct {                                                                     \
    T *items;                                                                  \
    size_t count;                                                              \
  }

#define view_foreach(Type, it, da)                                             \
  for (Type *it = (da)->items; it < (da)->items + (da)->count; ++it)

// ---------------------------------------------------------
// Types, these are here just to be in sync with the other code
// ---------------------------------------------------------

typedef uint64_t u64;
typedef int64_t s64;

typedef uint32_t u32;
typedef int32_t s32;

typedef uint16_t u16;
typedef int16_t s16;

typedef uint8_t u8;
typedef int8_t s8;

#if __SIZEOF_POINTER__ == 8
typedef u64 usize;
typedef s64 ssize;
#else
typedef u32 usize;
typedef s32 ssize;
#endif

typedef da(char) str;

#define str_fmt "%.*s"
#define str_arg(s) (int)(s)->count, (s)->items

#define str_append_cstr(str, cstr)                                             \
  do {                                                                         \
    const char *s = (cstr);                                                    \
    size_t n = strlen(s);                                                      \
    da_append_many(str, s, n);                                                 \
  } while (0)

typedef view(char) strv;

#define KB (1024)
#define MB (KB * KB)
#define GB (MB * KB)
#define TB (GB * KB)
// ---------------------------------------------------------
// Tmp buffers for easy string manipulation
// ---------------------------------------------------------

#ifndef TMP_BUF_SIZE
#define TMP_BUF_SIZE 8 * MB
#endif

char *talloc(u64 size);
void treset();
char *tsprintf(const char *fmt, ...);

// ---------------------------------------------------------
// Logging functions
// ---------------------------------------------------------
#define EACH_LOG(X)                                                            \
  X(INFO)                                                                      \
  X(WARN)                                                                      \
  X(ERR)                                                                       \
  X(LOG_NUM)

typedef_enum(log_level_t, EACH_LOG);

extern log_level_t minimal_log_level;
void plog(log_level_t level, const char *fmt, ...);

// ---------------------------------------------------------
// CMDs builder & runner
// ---------------------------------------------------------
typedef int fd;
typedef struct {
  da(char *) c;

  pid_t pid;
  fd fdin;
  fd fdout;
  fd fderr;
} cmd_t;

typedef struct {
  fd fdin;
  fd fdout;
  fd fderr;
} redirect_t;

#define NEW_READ_PIPE -3
#define NEW_WRITE_PIPE -2

#define cmd_append(cmd, ...)                                                   \
  da_append_many(&(cmd)->c, ((const char *[]){__VA_ARGS__}),                   \
                 sizeof((const char *[]){__VA_ARGS__}) / sizeof(const char *))

#define CHILD_PID (0)
#define INVALID_PID (-1)

void cmd_free(cmd_t *cmd);
void cmd_reset(cmd_t *cmd);

#define cmd_run(cmd, ...) __cmd_run(cmd, ((redirect_t){__VA_ARGS__}))
bool __cmd_run(cmd_t *cmd, redirect_t r);

#define cmd_run_reset(cmd, ...)                                                \
  __cmd_run_reset(cmd, ((redirect_t){__VA_ARGS__}))
bool __cmd_run_reset(cmd_t *cmd, redirect_t r);

bool cmd_wait(cmd_t *cmd);
bool cmd_wait_reset(cmd_t *cmd);

#define cmd_run_async(cmd, ...) __cmd_run(cmd, ((redirect_t){__VA_ARGS__}))
bool __cmd_run_async(cmd_t *cmd, redirect_t r);

#define cmd_wait_all(...)                                                      \
  __cmd_wait_all(((const cmd_t *[]){__VA_ARGS__}),                             \
                 sizeof(((const cmd_t *[]){__VA_ARGS__})) / sizeof(cmd))
#define cmd_wait_all_reset(...)                                                \
  __cmd_wait_all_reset(((const cmd_t *[]){__VA_ARGS__}),                       \
                       /* sizeof(((const cmd_t *[]){__VA_ARGS__})) / sizeof(cmd)) */
bool __cmd_wait_all(int n, cmd_t waiters[static n]);
bool __cmd_wait_all_reset(int n, cmd_t waiters[static n]);

// --------------------------------------------------------
// File operations
// --------------------------------------------------------
bool read_file(const char *path, str *sb);
bool write_file(const char *path, const char *s);
bool read_fd(s32 fd, str *s);
bool file_rename(const char *old_path, const char *new_path);
bool file_delete(const char *path);

// --------------------------------------------------------
// Auto-rebuild
// --------------------------------------------------------
#define go_rebuild_urself(argc, argv, ...)                                     \
  __go_rebuild_urself(argc, argv, __FILE__, ##__VA_ARGS__, NULL)
void __go_rebuild_urself(int argc, char **argv, const char *source_path, ...);
s32 needs_rebuild(const char *output_path, const char **input_paths,
                  usize input_paths_count);

#ifdef BEAR
#define __BEAR "bear", "--append", "--",
#else
#define __BEAR
#endif

#define SANITIZERS "-ggdb", "-fsanitize=address", "-fsanitize=undefined"

#ifdef IMPLEMENTATIONS

// ---------------------------------------------------------
// Tmp buffers for easy string manipulation IMPLEMENTATION
// ---------------------------------------------------------
static char __tmpbuf[TMP_BUF_SIZE] = {};
static int curr_size = 0;

char *talloc(u64 size) {
  if (curr_size + size >= TMP_BUF_SIZE)
    return NULL;

  char *ptr = &__tmpbuf[curr_size];
  curr_size += size;
  return ptr;
}

void treset() { curr_size = 0; }

char *tsprintf(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  int n = vsnprintf(NULL, 0, fmt, args);
  va_end(args);

  expect(n >= 0);
  char *ptr = talloc(n + 1);
  expect(ptr != NULL && "Exceeded __tbuf capacity");

  va_start(args, fmt);
  n = vsnprintf(ptr, n + 1, fmt, args);
  va_end(args);

  ptr[n] = '\0';

  return ptr;
}
// ---------------------------------------------------------
// Logging functions
// ---------------------------------------------------------

log_level_t minimal_log_level = INFO;

void plog(log_level_t level, const char *fmt, ...) {
  if (level < minimal_log_level)
    return;

#define CASE(LEVEL)                                                            \
  case LEVEL:                                                                  \
    fprintf(stderr, "[" #LEVEL "] ");                                          \
    break;

  switch (level) {
    EACH_LOG(CASE);

  default:
    unreachable("LOG");
  }

  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  fprintf(stderr, "\n");
}

// ---------------------------------------------------------
// CMDs builder & runner
// ---------------------------------------------------------
void cmd_free(cmd_t *cmd) { free(cmd->c.items); }
void cmd_reset(cmd_t *cmd) {
  cmd->c.count = 0;
  if (cmd->fdin) {
    close(cmd->fdin);
    cmd->fdin = 0;
  }
  if (cmd->fdout) {
    close(cmd->fdout);
    cmd->fdout = 0;
  }
  if (cmd->fderr) {
    close(cmd->fderr);
    cmd->fderr = 0;
  }
}

bool __cmd_run_reset(cmd_t *cmd, redirect_t r) {
  bool ret = __cmd_run(cmd, r);
  cmd->c.count = 0;
  return ret;
}

bool __cmd_run(cmd_t *cmd, redirect_t r) {
  __cmd_run_async(cmd, r);
  expect(cmd->pid != INVALID_PID);
  return cmd_wait(cmd);
}

bool cmd_wait(cmd_t *cmd) {
  pid_t pid = cmd->pid;

  if (pid == INVALID_PID)
    return false;

  while (true) {
    int wstatus = 0;
    if (waitpid(pid, &wstatus, 0) < 0) {
      plog(ERR, "could not wait on command (pid %d): %s", pid, strerror(errno));

      return false;
    }

    if (WIFEXITED(wstatus)) {
      int exit_status = WEXITSTATUS(wstatus);
      if (exit_status != 0) {
        plog(ERR, "command exited with exit code %d", exit_status);

        return false;
      }

      break;
    }

    if (WIFSIGNALED(wstatus)) {
      plog(ERR, "command process was terminated by signal %d",
           WTERMSIG(wstatus));
      return false;
    }
  }

  return true;
}

bool cmd_wait_reset(cmd_t *cmd) {
  bool r = cmd_wait(cmd);
  cmd_reset(cmd);
  return r;
}

void cmd_render(cmd_t *cmd, str *render) {
  for (size_t i = 0; i < cmd->c.count; ++i) {
    const char *arg = cmd->c.items[i];
    if (arg == NULL)
      break;
    if (i > 0)
      str_append_cstr(render, " ");
    if (!strchr(arg, ' ')) {
      str_append_cstr(render, arg);
    } else {
      da_append(render, '\'');
      str_append_cstr(render, arg);
      da_append(render, '\'');
    }
  }
}

void open_if_requested(fd to_open, fd new_pipes[2]) {
  if (to_open == NEW_WRITE_PIPE || to_open == NEW_READ_PIPE) {
    if (pipe(&new_pipes[0]) == -1) {
      plog(ERR, "Could not open a pipe %s\n", strerror(errno));
      exit(1);
    }
  }
}

void child_write_pipe(fd *to_open, fd new_pipes[2], int type) {
  if (*to_open == NEW_WRITE_PIPE) {
    close(new_pipes[1]);
    *to_open = new_pipes[0];
  }
  if (*to_open) {
    if (dup2(*to_open, type) < 0) {
      plog(ERR, "Could not setup stdin for child process: %s", strerror(errno));
      exit(1);
    }
  }
}

void child_read_pipe(fd *to_open, fd new_pipes[2], int type) {
  if (*to_open == NEW_READ_PIPE) {
    close(new_pipes[0]);
    *to_open = new_pipes[1];
  }
  if (*to_open) {
    if (dup2(*to_open, type) < 0) {
      plog(ERR, "Could not setup stdin for child process: %s", strerror(errno));
      exit(1);
    }
  }
}

void parent_write_pipe(fd *to_open, fd new_pipes[2]) {
  if (*to_open == NEW_WRITE_PIPE) {
    close(new_pipes[0]);
    *to_open = new_pipes[1];
  }
}
void parent_read_pipe(fd *to_open, fd new_pipes[2]) {
  if (*to_open == NEW_READ_PIPE) {
    close(new_pipes[1]);
    *to_open = new_pipes[0];
  }
}

bool __cmd_run_async(cmd_t *c, redirect_t r) {
  fd pipefd[3][2];

  if (r.fdin != 0)
    c->fdin = r.fdin;

  if (r.fdout != 0)
    c->fdout = r.fdout;

  if (r.fderr != 0)
    c->fderr = r.fderr;

  if (c->c.count < 1) {
    plog(ERR, "Could not run empty command");
    return false;
  }

  str s = {0};
  cmd_render(c, &s);
  da_append(&s, '\0');

  plog(INFO, "CMD: %s", s.items);
  da_free(&s);

  memset(&s, 0, sizeof(s));

  open_if_requested(c->fdin, pipefd[STDIN_FILENO]);
  open_if_requested(c->fdout, pipefd[STDOUT_FILENO]);
  open_if_requested(c->fderr, pipefd[STDERR_FILENO]);

  pid_t cpid = fork();
  if (cpid < 0) {
    plog(ERR, "Could not fork child process: %s", strerror(errno));
    return -1;
  }

  if (cpid == CHILD_PID) {
    child_write_pipe(&c->fdin, pipefd[STDIN_FILENO], STDIN_FILENO);
    child_read_pipe(&c->fdout, pipefd[STDOUT_FILENO], STDOUT_FILENO);
    child_read_pipe(&c->fderr, pipefd[STDERR_FILENO], STDERR_FILENO);

    cmd_t to_run = {0};

    da_append_many(&to_run.c, c->c.items, c->c.count);
    cmd_append(&to_run, NULL);

    expect(execvp(to_run.c.items[0], (char *const *)to_run.c.items) < 0 &&
           tsprintf("Failed to launch a new process because: %s",
                    strerror(errno)));

    unreachable("run async redirect");
  }

  parent_write_pipe(&c->fdin, pipefd[STDIN_FILENO]);
  parent_read_pipe(&c->fdout, pipefd[STDOUT_FILENO]);
  parent_read_pipe(&c->fderr, pipefd[STDERR_FILENO]);

  return cpid;
}

bool __cmd_wait_all(int n, cmd_t waiters[static n]) {
  bool r = true;
  for (size_t i = 0; i < n; ++i) {
    r = cmd_wait(&waiters[i]) && r;
  }
  return r;
}

bool __cmd_wait_all_reset(int n, cmd_t waiters[static n]) {
  bool r = true;
  for (size_t i = 0; i < n; ++i) {
    r = cmd_wait_reset(&waiters[i]) && r;
  }
  return r;
}

bool read_file(const char *path, str *s) {
  FILE *f = fopen(path, "rb");
  if (f == NULL) {
    goto fail;
  }

  if (fseek(f, 0, SEEK_END) < 0)
    goto fail_clean;

  u64 len = ftell(f);
  if (len < 0)
    goto fail_clean;

  if (fseek(f, 0, SEEK_SET) < 0)
    goto fail_clean;

  da_reserve(s, s->count + len);
  u64 read = fread(s->items + s->count, len, 1, f);
  if (read != 1)
    goto fail_read;

  if (ferror(f))
    goto fail_clean;

  s->count += len;
  s->items[s->count - 1] = '\0';

  return true;

fail_clean:
  fclose(f);
fail:
  printf("Could not read file %s: %s", path, strerror(errno));
  return false;

fail_read:
  if (feof(f)) {
    fprintf(stderr, "Error: Unexpected end of file\n");
  } else if (ferror(f)) {
    perror("Error reading file");
  } else {
    fprintf(stderr,
            "Error: fread read fewer items than requested read: %ld != "
            "requested %d\n",
            read, 1);
  }

  fclose(f);
  return false;
}

bool write_file(const char *path, const char *s) {
  fd f = open(path, O_WRONLY | O_CREAT | O_TRUNC,
              S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if (f < 0) {
    goto fail;
  }

  int len = strlen(s);
  int written = write(f, s, len);
  if (written != len) {
    goto fail_clean;
  }

  return true;

fail_clean:
  close(f);

fail:
  printf("Could not write file %s: %s", path, strerror(errno));
  return false;
}

bool read_fd(int fd, str *s) {
  const size_t chunk_size = 4096;
  char temp[chunk_size];

  while (1) {
    ssize_t n = read(fd, temp, chunk_size);
    if (n < 0) {
      if (errno == EINTR)
        continue;
      perror("read");
      return false;
    }
    if (n == 0)
      break;

    da_reserve(s, s->count + n + 1);
    memcpy(s->items + s->count, temp, n);
    s->count += n;
  }

  if (s->count > 0)
    s->items[s->count] = '\0';

  return true;
}

bool file_rename(const char *old_path, const char *new_path) {
  plog(INFO, "renaming %s -> %s", old_path, new_path);
  if (rename(old_path, new_path) < 0) {
    plog(ERR, "could not rename %s to %s: %s", old_path, new_path,
         strerror(errno));
    return false;
  }
  return true;
}

bool file_delete(const char *path) {
  plog(INFO, "deleting %s", path);
  if (remove(path) < 0) {
    plog(ERR, "Could not delete file %s: %s", path, strerror(errno));
    return false;
  }
  return true;
}

/* typedef da_str da(str); */
// The implementation idea is stolen from https://github.com/zhiayang/nabs
// And from nob.h
void __go_rebuild_urself(int argc, char **argv, const char *source_path, ...) {
  const char *binary_path = shift(argv, argc);

  da(const char *) source_paths = {0};
  da_append(&source_paths, source_path);
  va_list args;
  va_start(args, source_path);
  for (;;) {
    const char *path = va_arg(args, const char *);
    if (path == NULL)
      break;
    da_append(&source_paths, path);
  }
  va_end(args);

  int rebuild_is_needed =
      needs_rebuild(binary_path, source_paths.items, source_paths.count);

  if (rebuild_is_needed < 0)
    exit(1);                // error
  if (!rebuild_is_needed) { // no rebuild is needed
    free(source_paths.items);
    return;
  }

  cmd_t c = {0};

  const char *old_binary_path = tsprintf("%s.old", binary_path);

  if (!file_rename(binary_path, old_binary_path))
    exit(1);

  cmd_append(&c, "cc", "-o", binary_path, source_path);
  if (!cmd_run_reset(&c)) {
    file_rename(old_binary_path, binary_path);
    exit(1);
  }
  file_delete(old_binary_path);

  cmd_append(&c, binary_path);
  da_append_many(&c.c, argv, argc);

  if (!cmd_run_reset(&c))
    exit(1);

  exit(0);
}

s32 needs_rebuild(const char *output_path, const char **input_paths,
                  usize input_paths_count) {
  struct stat statbuf = {0};

  if (stat(output_path, &statbuf) < 0) {
    // NOTE: if output does not exist it 100% must be rebuilt
    if (errno == ENOENT)
      return 1;

    plog(ERR, "could not stat %s: %s", output_path, strerror(errno));
    return -1;
  }
  s32 output_path_time = statbuf.st_mtime;

  for (size_t i = 0; i < input_paths_count; ++i) {
    const char *input_path = input_paths[i];
    if (stat(input_path, &statbuf) < 0) {
      // NOTE: non-existing input is an error cause it is needed for building in
      // the first place
      plog(ERR, "could not stat %s: %s", input_path, strerror(errno));
      return -1;
    }
    int input_path_time = statbuf.st_mtime;
    // NOTE: if even a single input_path is fresher than output_path that's 100%
    // rebuild
    if (input_path_time > output_path_time)
      return 1;
  }

  return 0;
}

#endif // IMPLEMENTATIONS
#endif // _LBSTD
