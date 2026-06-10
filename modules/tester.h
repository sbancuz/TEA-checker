#define _GNU_SOURCE
#ifndef _TESTER
#define _TESTER

#ifndef TEST_NAME
#ifndef ORCHESTRATOR // TODO: This is a hack
#error Missing TEST_NAME, please import test_name.h.out before tester.h
#endif
#endif

#define EXPAND(x) x
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define AS_RESULT(x)                                                           \
  typedef x result_t;                                                          \
  static x *RESULT

#define CAT3(a, b, c) CAT3_IMPL(a, b, c)
#define CAT3_IMPL(a, b, c) a##b##_##c

typedef enum {
  OK = 0,
  KO = 1,
  RETRY = 2,
} result_code_t;

#include "commands.h"

#define EXPORT_RESULT_STRUCT_SIZE() u64 CAT3(TEST_NAME, _result, size)(void)

#define EXPORT_RESULT_STRUCT_DIAGNOSTICS(S)                                    \
  result_code_t CAT3(TEST_NAME, _result, diagnostics)(S)

#define EXPORT_RESULT_SETUP(S) result_code_t CAT3(TEST_NAME, _result, setup)(S)

#define X "x"
#define Y "y"
#define STR(x) #x

#define AXIS(xy, name, val) __attribute__((annotate(xy "=" name ":" STR(val))))
#define VALUES(name) __attribute__((annotate("values=" name)))
#define RANGE(x) AXIS("range", x)
#define TO_PLOT(kind, n)                                                       \
  __attribute__((annotate("to_plot=" kind ","                                  \
                          "name=" n)))

#endif // _TESTER
