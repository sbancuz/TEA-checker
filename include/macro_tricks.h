#ifndef __MACRO_TRICKS
#define __MACRO_TRICKS

#define apply(m, x) m(x)
#define apply5(m, x)                                                           \
  apply(m, x);                                                                 \
  apply(m, x);                                                                 \
  apply(m, x);                                                                 \
  apply(m, x);                                                                 \
  apply(m, x)

#define apply25(m, x)                                                          \
  apply5(m, x);                                                                \
  apply5(m, x);                                                                \
  apply5(m, x);                                                                \
  apply5(m, x);                                                                \
  apply5(m, x)

#define apply125(m, x)                                                         \
  apply25(m, x);                                                               \
  apply25(m, x);                                                               \
  apply25(m, x);                                                               \
  apply25(m, x);                                                               \
  apply25(m, x)

#define apply625(m, x)                                                         \
  apply125(m, x);                                                              \
  apply125(m, x);                                                              \
  apply125(m, x);                                                              \
  apply125(m, x);                                                              \
  apply125(m, x)
#endif
