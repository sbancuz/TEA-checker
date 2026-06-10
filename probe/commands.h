#ifndef _PROBE_COMMANDS
#define _PROBE_COMMANDS
#include "../include/types.h" // TODO: Import it better

enum probe_command {
  PROBE_GET = 1,
  PROBE_CACHE = 2,
  PROBE_UNCACHE = 3,
  PROBE_TLB_FLUSH = 4,
  PROBE_TLB_FLUSH_PAGE = 5,
  PROBE_TLB_REMOVE_PTE = 6,
  PROBE_TLB_RESTORE_PTE = 7,
};

struct probe_request {
  usize *ret;
  usize *access_time;
};

#endif
