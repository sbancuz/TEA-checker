#ifndef _stl_forward_TEST
#define _stl_forward_TEST

#include "test_name.h.out"

#include "../tester.h"
#include "types.h"

#define OFFSETS 64

#define X "x"
#define Y "y"
#define STR(x) #x

#define AXIS(xy, name, val) __attribute__((annotate(xy "=" name ":" STR(val))))
#define VALUES(name) __attribute__((annotate("values=" name)))
#define RANGE(x) AXIS("range", x)
#define TO_PLOT(kind, n)                                                       \
  __attribute__((annotate("to_plot=" kind ","                                  \
                          "name=" n)))

typedef struct {
  double overhead;
  usize iterations;

  usize readings[OFFSETS * OFFSETS];
  double timings[OFFSETS * OFFSETS] TO_PLOT("heatmap", "stl_timings")
      AXIS(X, "load_offset", OFFSETS) AXIS(Y, "store_offset", OFFSETS)
          VALUES("latency");
} stl_forward_result_t;

#endif
