#ifndef _LBSTD
#define _LBSTD

#include <argp.h>
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <float.h>
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
  ({                                                                           \
    da_reserve((da), (da)->count + 1);                                         \
    (da)->items[(da)->count++] = (item);                                       \
    &((da)->items[(da)->count - 1]);                                           \
  })

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

#define da_foreach_s(Type, it, da)                                             \
  for (Type *it = (da).items; it < (da).items + (da).count; ++it)

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

#ifndef _TYPES
#define _TYPES
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
usize tsave();
char *tconcat(const char *, ...);
void trestore(usize checkpoint);
char *tsprintf(const char *fmt, ...);
char *tstrdup(const char *s);

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
#define plog(level, fmt, ...)                                                  \
  __plog(level, __FILE__ ":" TOSTRING(__LINE__) ":", __func__, fmt,            \
         ##__VA_ARGS__)

void __plog(log_level_t level, const char *prefix, const char *func,
            const char *fmt, ...);

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

bool read_until_close(fd f, str *out);
// --------------------------------------------------------
// File operations
// --------------------------------------------------------
bool read_file(const char *path, str *sb);
bool write_to_file(const char *path, const char *s);
bool write_to_file_bin(const char *path, const u8 *s, usize len);
bool read_fd(s32 fd, str *s);
bool file_rename(const char *old_path, const char *new_path);
bool file_delete(const char *path);
typedef da(const char *) paths_t;
bool read_dir(const char *parent, paths_t *children);

// --------------------------------------------------------
// Bin parser
// --------------------------------------------------------
usize bp_peek_usize(const u8 *ptr);
usize bp_get_usize(u8 **ptr);

const char *bp_peek_string(const u8 *ptr);
const char *bp_get_string(u8 **ptr);

u8 bp_peek_u8(const u8 *ptr);
u8 bp_get_u8(u8 **ptr);

int bp_peek_int(const u8 *ptr);
int bp_get_int(u8 **ptr);

char *bp_get_bytes(u8 **ptr, usize len);

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
// --------------------------------------------------------
// Math
// --------------------------------------------------------

s32 detect_jumps_sliding(const double *data, int n, int window, double percent);
bool are_close(double a, double b, double percent);
static inline double fabs(double x);
static double mean(const double *data, int start, int len);
static double median(double arr[], int n);
static inline u64 sqrt_u(u64);

ssize detect_jump_cusum(int n, double data[n], double threshold);
double sliding_median(int w, double window[w]);
void median_filter(int n, double data[n], double filtered[n], s32 window);
s32 detect_jump_welch(s32 n, const double data[n], s32 window,
                      double threshold);
ssize jump_welch_rel(s32 n, const double data[n], s32 window, double percent);
double sqrt_d(double x);

#ifdef IMPLEMENTATIONS
// ---------------------------------------------------------
// Tmp buffers for easy string manipulation IMPLEMENTATION
// ---------------------------------------------------------
static char __tmpbuf[TMP_BUF_SIZE] = {};
static usize __tmpbuf_curr_size = 0;

char *talloc(u64 size) {
  if (__tmpbuf_curr_size + size >= TMP_BUF_SIZE)
    return NULL;

  char *ptr = &__tmpbuf[__tmpbuf_curr_size];
  __tmpbuf_curr_size += size;
  return ptr;
}

void treset() { __tmpbuf_curr_size = 0; }

usize tsave() { return __tmpbuf_curr_size; }
void trestore(usize checkpoint) { __tmpbuf_curr_size = checkpoint; }

char *tconcat(const char *s, ...) {
  if (!s)
    return NULL;

  va_list args;

  usize total = strlen(s);
  va_start(args, s);
  const char *arg;
  while ((arg = va_arg(args, const char *)) != NULL) {
    total += strlen(arg);
  }
  va_end(args);

  // Allocate
  char *ptr = talloc(total + 1);
  expect(ptr != NULL && "Exceeded __tbuf capacity");

  // Second pass: copy strings
  char *dst = ptr;
  memcpy(dst, s, strlen(s));
  dst += strlen(s);

  va_start(args, s);
  while ((arg = va_arg(args, const char *)) != NULL) {
    usize len = strlen(arg);
    memcpy(dst, arg, len);
    dst += len;
  }
  va_end(args);

  *dst = '\0';
  return ptr;
}

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

char *tstrdup(const char *s) {
  usize len = strlen(s);

  char *ptr = talloc(len + 1);
  memcpy(ptr, s, len);
  ptr[len] = 0;

  return ptr;
}
// ---------------------------------------------------------
// Logging functions
// ---------------------------------------------------------

log_level_t minimal_log_level = INFO;

void __plog(log_level_t level, const char *prefix, const char *func,
            const char *fmt, ...) {
  if (level < minimal_log_level)
    return;

#define CASE(LEVEL)                                                            \
  case LEVEL:                                                                  \
    fprintf(stderr, "[" #LEVEL "] ");                                          \
    break;

  if (level == ERR)
    fprintf(stderr, "(%s%s) ", prefix, func);

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
      plog(ERR, "Could not setup pipe for child process: %s", strerror(errno));
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
  for (usize i = 0; i < (usize)n; ++i) {
    r = cmd_wait(&waiters[i]) && r;
  }
  return r;
}

bool __cmd_wait_all_reset(int n, cmd_t waiters[static n]) {
  bool r = true;
  for (usize i = 0; i < (usize)n; ++i) {
    r = cmd_wait_reset(&waiters[i]) && r;
  }
  return r;
}

bool read_until_close(fd f, str *out) {
  ssize_t n;
  char buf[4096];

  while (1) {
    n = read(f, buf, sizeof buf);

    if (n > 0) {
      da_append_many(out, buf, n);
    } else if (n == 0) {
      // EOF: all writers have closed the pipe
      break;
    } else {
      // n < 0 → error
      if (errno == EINTR)
        continue; // interrupted, retry
      perror("read");
      return false;
    }
  }

  return true;
}

bool read_file(const char *path, str *s) {
  FILE *f = fopen(path, "rb");
  if (f == NULL) {
    goto fail;
  }

  if (fseek(f, 0, SEEK_END) < 0)
    goto fail_clean;

  u64 len = ftell(f);
  if (len == -1ULL)
    goto fail_clean;

  if (fseek(f, 0, SEEK_SET) < 0)
    goto fail_clean;

  da_reserve(s, s->count + len + 1);
  u64 read = fread(s->items + s->count, len, 1, f);
  if (read != 1)
    goto fail_read;

  if (ferror(f))
    goto fail_clean;

  s->count += len;
  s->items[s->count] = '\0';

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
            "Error: fread read fewer items than requested read: %lld != "
            "requested %d\n",
            (long long int)read, 1);
  }

  fclose(f);
  return false;
}

bool write_to_file(const char *path, const char *s) {
  return write_to_file_bin(path, s, strlen(s));
}

bool write_to_file_bin(const char *path, const u8 *s, usize len) {
  fd f = open(path, O_WRONLY | O_CREAT | O_TRUNC,
              S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if (f < 0) {
    goto fail;
  }

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

bool read_dir(const char *parent, paths_t *children) {
  bool result = true;
  DIR *dir = NULL;
  struct dirent *ent = NULL;

  dir = opendir(parent);
  if (dir == NULL) {
    plog(ERR, "Could not open directory %s: %s", parent, strerror(errno));

    result = false;
    goto defer;
  }

  errno = 0;
  ent = readdir(dir);
  while (ent != NULL) {
    da_append(children, tstrdup(ent->d_name));
    ent = readdir(dir);
  }

  if (errno != 0) {
    plog(ERR, "Could not read directory %s: %s", parent, strerror(errno));

    result = false;
    goto defer;
  }

defer:
  if (dir)
    closedir(dir);
  return result;
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

inline usize bp_peek_usize(const u8 *ptr) {
  usize v;
  memcpy(&v, ptr, sizeof(v));
  return v;
}

inline usize bp_get_usize(u8 **ptr) {
  usize v = bp_peek_usize(*ptr);
  *ptr += sizeof(usize);
  return v;
}

inline const char *bp_peek_string(const u8 *ptr) { return (const char *)ptr; }
inline const char *bp_get_string(u8 **ptr) {
  const char *s = (const char *)*ptr;
  size_t len = strlen(s) + 1;
  *ptr += len;
  return s;
}

inline u8 bp_peek_u8(const u8 *ptr) { return *ptr; }
inline u8 bp_get_u8(u8 **ptr) {
  u8 v = **ptr;
  *ptr += 1;
  return v;
}

inline int bp_peek_int(const u8 *ptr) {
  int v;
  memcpy(&v, ptr, sizeof(v));
  return v;
}

inline int bp_get_int(u8 **ptr) {
  int v = bp_peek_int(*ptr);
  *ptr += sizeof(int);
  return v;
}

inline char *bp_get_bytes(u8 **ptr, usize len) {
  char *r = (char *)*ptr;
  *ptr += len;
  return r;
}

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
      // NOTE: non-existing input is an error cause it is needed for building
      // in the first place
      plog(ERR, "could not stat %s: %s", input_path, strerror(errno));
      return -1;
    }
    int input_path_time = statbuf.st_mtime;
    // NOTE: if even a single input_path is fresher than output_path that's
    // 100% rebuild
    if (input_path_time > output_path_time)
      return 1;
  }

  return 0;
}

//
static inline double fabs(double x) { return (x < 0.0) ? -x : x; }

static double mean(const double *data, int start, int len) {
  double sum = 0.0;
  for (int i = start; i < start + len; i++)
    sum += data[i];
  return sum / len;
}

bool are_close(double a, double b, double percent) {
  if (a == 0.0 && b == 0.0)
    return true;

  double diff = fabs(a - b);
  double avg = (fabs(a) + fabs(b)) / 2.0;

  return diff <= (avg * (percent / 100.0));
}

/* // Comparison function for qsort */
/* int cmp_u64(const void *a, const void *b) { */
/*   u64 x = *(const u64 *)a; */
/*   u64 y = *(const u64 *)b; */
/*   if (x < y) */
/*     return -1; */
/*   if (x > y) */
/*     return 1; */
/*   return 0; */
/* } */
/*  */
/* int cmp_double(const void *a, const void *b) { */
/*   double x = *(const double *)a; */
/*   double y = *(const double *)b; */
/*   if (x < y) */
/*     return -1; */
/*   if (x > y) */
/*     return 1; */
/*   return 0; */
/* } */
/*  */
/* // Median function */
/* double median(double arr[], int n) { */
/*   qsort(arr, n, sizeof(u64), cmp_double); */
/*   double med; */
/*   if (n % 2 == 0) { */
/*     med = (arr[n / 2 - 1] + arr[n / 2]) / 2; */
/*   } else { */
/*     med = arr[n / 2]; */
/*   } */
/*  */
/*   return med; */
/* } */
/*  */
/* // Simple linear regression to get slope and intercept */
/* void linear_trend(const double *data, int start, int len, double *slope, */
/*                   double *intercept) { */
/*   double sum_x = 0.0, sum_y = 0.0, sum_x2 = 0.0, sum_xy = 0.0; */
/*   for (int i = 0; i < len; i++) { */
/*     sum_x += i; */
/*     sum_y += data[start + i]; */
/*     sum_x2 += i * i; */
/*     sum_xy += i * data[start + i]; */
/*   } */
/*   double denom = len * sum_x2 - sum_x * sum_x; */
/*   if (denom == 0.0) { */
/*     *slope = 0.0; */
/*     *intercept = sum_y / len; */
/*   } else { */
/*     *slope = (len * sum_xy - sum_x * sum_y) / denom; */
/*     *intercept = (sum_y - (*slope) * sum_x) / len; */
/*   } */
/* } */
/*  */
/* static inline double predict(double slope, double intercept, int x) { */
/*   return slope * x + intercept; */
/* } */
/*  */
/* double mean_residual(const double *data, int start, int len, double slope, */
/*                      double intercept) { */
/*   double sum = 0.0; */
/*   for (int i = 0; i < len; i++) { */
/*     sum += data[start + i] - predict(slope, intercept, i); */
/*   } */
/*   return sum / len; */
/* } */
/*  */
/* s32 detect_jumps_sliding_median(const double *data, int n, int window, */
/*                                 double percent) { */
/*   if (window * 2 >= n) { */
/*     printf("Window too large.\n"); */
/*     return -1; */
/*   } */
/*  */
/*   for (int i = window; i < n - window; i++) { */
/*     /\* /\\* double left_mean = mean(data, i - window, window); *\\/ *\/ */
/*     /\* /\\* double right_mean = mean(data, i, window); *\\/ *\/ */
/*     double left_median = median(data + i - window, window); */
/*     double right_median = median(data + i, window); */
/*     /\*  *\/ */
/*     if (!are_close(data[i] - right_median, data[i] - left_median, percent)) {
 */
/*       return i; */
/*     } */
/*   } */
/*  */
/*   return -1; */
/* } */
/*  */
/* s32 detect_jumps_sliding(const double *data, int n, int window, */
/*                          double percent) { */
/*   if (window * 2 >= n) { */
/*     printf("Window too large.\n"); */
/*     return -1; */
/*   } */
/*  */
/*   for (int i = window; i < n - window; i++) { */
/*     double slope_left, intercept_left; */
/*     double slope_right, intercept_right; */
/*  */
/*     // Fit linear trend to left and right windows */
/*     linear_trend(data, i - window, window, &slope_left, &intercept_left); */
/*     linear_trend(data, i, window, &slope_right, &intercept_right); */
/*  */
/*     double left_res = */
/*         mean_residual(data, i - window, window, slope_left, intercept_left);
 */
/*     double right_res = */
/*         mean_residual(data, i, window, slope_right, intercept_right); */
/*  */
/*     if (!are_close(left_res, right_res, percent)) { */
/*       return i; // jump detected */
/*     } */
/*   } */
/*  */
/*   return -1; */
/* } */

double sqrt_d(double x) {
  if (x <= 0.0)
    return 0.0;

  double guess = x * 0.5;
  double prev;

  for (int i = 0; i < 20; i++) {
    prev = guess;
    guess = 0.5 * (guess + x / guess);
    if (guess == prev)
      break;
  }
  return guess;
}

static double welch_df(double varL, double varR, int n) {
  double a = varL / n;
  double b = varR / n;

  double num = (a + b) * (a + b);
  double den = (a * a) / (n - 1) + (b * b) / (n - 1);

  if (den <= DBL_EPSILON)
    return 1.0;

  return num / den;
}

s32 detect_jump_welch(s32 n, const double data[n], s32 window,
                      double t_critical) {
  if (window < 2 || n < 2 * window)
    return -1;

  double sumL = 0.0, sumR = 0.0;
  double sumsqL = 0.0, sumsqR = 0.0;

  for (s32 i = 0; i < window; i++) {
    sumL += data[i];
    sumsqL += data[i] * data[i];
  }

  for (s32 i = window; i < 2 * window; i++) {
    sumR += data[i];
    sumsqR += data[i] * data[i];
  }

  s32 out_len = n - 2 * window + 1;

  for (s32 i = 0; i < out_len; i++) {

    double meanL = sumL / window;
    double meanR = sumR / window;

    double varL = (sumsqL - (sumL * sumL) / window) / (window - 1);
    double varR = (sumsqR - (sumR * sumR) / window) / (window - 1);

    /* Clamp negative variance due to FP error */
    if (varL < 0.0)
      varL = 0.0;
    if (varR < 0.0)
      varR = 0.0;

    double denom = sqrt_d(varL / window + varR / window);

    if (denom > 0.0) {

      double t = (meanL - meanR) / denom;
      double df = welch_df(varL, varR, window);

      /* Compare against two-sided critical value */
      if (fabs(t) > t_critical)
        return i + window;
    }

    /* Slide windows */
    if (i + 2 * window < n) {

      double oldL = data[i];
      double newL = data[i + window];

      sumL += newL - oldL;
      sumsqL += newL * newL - oldL * oldL;

      double oldR = data[i + window];
      double newR = data[i + 2 * window];

      sumR += newR - oldR;
      sumsqR += newR * newR - oldR * oldR;
    }
  }

  return -1;
}

ssize jump_welch_rel(s32 n, const double data[n], s32 window, double percent) {
  if (window < 2 || n < 2 * window)
    return -1;

  double sumL = 0.0, sumR = 0.0;
  for (s32 i = 0; i < window; i++) {
    sumL += data[i];
  }
  for (s32 i = window; i < 2 * window; i++) {
    sumR += data[i];
  }

  double baseline = sumL / window;
  if (baseline < 1.0)
    baseline = 1.0;
  double threshold = fabs(baseline) * percent / 100.0;

  s32 out_len = n - 2 * window + 1;
  for (s32 i = 0; i < out_len; i++) {
    double meanL = sumL / window;
    double meanR = sumR / window;

    if (fabs(meanR - meanL) > threshold)
      return i + window;

    if (i + 2 * window < n) {
      sumL += data[i + window] - data[i];
      sumR += data[i + 2 * window] - data[i + window];
    }
  }
  return -1;
}

ssize detect_jump_cusum(s32 n, double data[n], double threshold) {
  double S_pos = 0.0;
  double S_neg = 0.0;

  double m = mean(data, 0, n / 10);
  double t = fabs(m) * (threshold / (100));
  double k = t / 2;

  for (s32 i = 0; i < n; i++) {
    S_pos = S_pos + (data[i] - m - k);
    if (S_pos < 0)
      S_pos = 0;

    S_neg = S_neg + (m - data[i] - k);
    if (S_neg < 0)
      S_neg = 0;

    if (S_pos > t || S_neg > t) {
      return i;
    }
  }

  return -1;
}

double sliding_median(s32 w, double window[w]) {
  double temp[w];

  // Copy data into temp buffer
  for (s32 i = 0; i < w; ++i)
    temp[i] = window[i];

  // Insertion sort on temp
  for (s32 i = 1; i < w; ++i) {
    double key = temp[i];
    s32 j = i - 1;

    while (j >= 0 && temp[j] > key) {
      temp[j + 1] = temp[j];
      j--;
    }
    temp[j + 1] = key;
  }

  return temp[w / 2];
}

void median_filter(int n, double data[n], double filtered[n], s32 window) {
  filtered[0] = data[0];
  for (int i = 1; i < n - 1; i++)
    filtered[i] = sliding_median(window, &data[i - 1]);
}

static inline u64 sqrt_u(u64 n) {
  u64 res = 0;
  u64 bit = (u64)1 << 62;

  while (bit > n)
    bit >>= 2;

  while (bit != 0) {
    if (n >= res + bit) {
      n -= res + bit;
      res = (res >> 1) + bit;
    } else {
      res >>= 1;
    }
    bit >>= 2;
  }

  return res;
}

#endif // IMPLEMENTATIONS
#endif // _LBSTD
