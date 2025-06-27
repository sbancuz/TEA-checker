#ifndef _TESTER
#define _TESTER

#ifndef TEST_NAME
#error Missing TEST_NAME, please import test_name.h.out before tester.h
#endif

#define EXPAND(x) x
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define DEFINE_RESULT_STRUCT struct __result
#define RESULT _result
#define DECLARE_RESULT DEFINE_RESULT_STRUCT *RESULT
#define SIZEOF_RESULT() sizeof(EXPAND(DEFINE_RESULT_STRUCT))

#define CAT3(a, b, c) CAT3_IMPL(a, b, c)
#define CAT3_IMPL(a, b, c) a##b##_##c

#define __SIZE(a, b) CAT3(a, b, size)
#define __DIAGNOSTICS(a, b) CAT3(a, b, diagnostics)

#define EXPORT_RESULT_STRUCT_SIZE()                                            \
  u64 __SIZE(TEST_NAME, RESULT)(void) { return sizeof(DEFINE_RESULT_STRUCT); }

#define EXPORT_RESULT_STRUCT_DIAGNOSTICS(S)                                    \
  bool __DIAGNOSTICS(TEST_NAME, RESULT)(S)

#endif // _TESTER
