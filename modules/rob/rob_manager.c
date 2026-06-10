#include "../commands.h"
#include "rob_test.h"

#define IMPLEMENTATIONS
#include "../../libs/lbstd.h"
#include <stdlib.h>

EXPORT_RESULT_SETUP(request_dependencies_t *dependencies) {
  usize max_size = 512;
  usize *iterations = dependencies[0];
  *iterations = sqrt_u(*iterations);
  str instructions = {0};

  const char *ifmt = "#define itest%d \\\n"
                     "for (s32 i = 0; i < RESULT->iterations; i++) {\\\n"
                     "cache_line_flush(ptr1);\\\n"
                     "cache_line_flush(ptr2);\\\n"
                     "serialise(); memory_barrier();\\\n"
                     "volatile usize start = get_cycle();\\\n"
                     /* "__asm__ __volatile__ (ninst%d);\\\n" */
                     "ninst%d;\\\n"
                     "load(ptr1);\\\n"
                     /* "__asm__ __volatile__ (ninst%d);\\\n" */
                     "ninst%d;\\\n"
                     "load(ptr2);\\\n"
                     /* "__asm__ __volatile__ (ninst%d);\\\n" */
                     "ninst%d;\\\n"
                     "volatile usize end = get_cycle();\\\n"
                     "RESULT->raw_readings_nop[%d] += end - start;\\\n"
                     "}\n";

  const char *ifmt_mitigate =
      "#define itest%d \\\n"
      "for (s32 i = 0; i < RESULT->iterations; i++) {\\\n"
      "cache_line_flush(ptr1);\\\n"
      "cache_line_flush(ptr2);\\\n"
      "serialise(); memory_barrier();\\\n"
      "volatile usize start = get_cycle();\\\n"
      /* "__asm__ __volatile__ (ninst%d);\\\n" */
      "ninst%d;\\\n"

      "serialise(); \\\n"

      "load(ptr1);\\\n"
      "serialise(); \\\n"

      /* "__asm__ __volatile__ (ninst%d);\\\n" */
      "ninst%d;\\\n"
      "serialise(); \\\n"
      "load(ptr2);\\\n"
      "serialise(); \\\n"
      /* "__asm__ __volatile__ (ninst%d);\\\n" */
      "ninst%d;\\\n"
      "serialise(); \\\n"
      "volatile usize end = get_cycle();\\\n"
      "RESULT->raw_readings_nop[%d] += end - start;\\\n"
      "}\n";

  /* const char *xfmt = "#define xtest%d \\\n" */
  /*                    "for (s32 i = 0; i < RESULT->iterations; i++) {\\\n" */
  /*                    "cache_line_flush(ptr1);\\\n" */
  /*                    "cache_line_flush(ptr2);\\\n" */
  /*                    "serialise(); memory_barrier();\\\n" */
  /*                    "volatile usize start = get_cycle();\\\n" */
  /*                    /\* "__asm__ __volatile__ (xinst%d:::\"ebx\");\\\n" *\/
   */
  /*                    "xinst%d;\\\n" */
  /*                    "load(ptr1);\\\n" */
  /*                    /\* "__asm__ __volatile__ (xinst%d:::\"ebx\");\\\n" *\/
   */
  /*                    "xinst%d;\\\n" */
  /*                    "load(ptr2);\\\n" */
  /*                    /\* "__asm__ __volatile__ (xinst%d:::\"ebx\");\\\n" *\/
   */
  /*                    "xinst%d;\\\n" */
  /*                    /\* "serialise(); memory_barrier();\\\n" *\/ */
  /*                    "volatile usize end = get_cycle();\\\n" */
  /*                    "RESULT->raw_readings_xor[%d] += end - start;\\\n" */
  /*                    "}\n"; */

  str_append_cstr(&instructions, tsprintf("unsigned long long __x_ = 0;\n"));
  str_append_cstr(&instructions, tsprintf("#define ninst1 nop_nomem;\n"));
  /* str_append_cstr(&instructions, tsprintf("#define xinst1 add(__x_)\n")); */

  str_append_cstr(&instructions, "#ifdef MITIGATE\n");
  str_append_cstr(&instructions, tsprintf(ifmt_mitigate, 1, 1, 1, 1, 1));
  str_append_cstr(&instructions, "#else\n");
  str_append_cstr(&instructions, tsprintf(ifmt, 1, 1, 1, 1, 1));
  str_append_cstr(&instructions, "#endif\n");
  /* str_append_cstr(&instructions, tsprintf(xfmt, 1, 1, 1, 1, 1)); */
  for (s32 i = 2; i < max_size; i++) {
    str_append_cstr(&instructions,
                    tsprintf("#define ninst%d ninst%d ninst1\n", i, i - 1));
    /* str_append_cstr(&instructions, */
    /*                 tsprintf("#define xinst%d xinst%d xinst1\n", i, i - 1));
     */

    str_append_cstr(&instructions, "#ifdef MITIGATE\n");
    str_append_cstr(&instructions, tsprintf(ifmt_mitigate, i, i, i, i, i));
    str_append_cstr(&instructions, "#else\n");
    str_append_cstr(&instructions, tsprintf(ifmt, i, i, i, i, i));
    str_append_cstr(&instructions, "#endif\n");
    /* str_append_cstr(&instructions, tsprintf(xfmt, i, i, i, i, i)); */
  }

  str_append_cstr(&instructions, tsprintf("#define run_battery_i "));
  for (s32 i = 1; i < max_size; i++) {
    str_append_cstr(&instructions, tsprintf("itest%d ", i));
  }
  str_append_cstr(&instructions, tsprintf("\n"));
  /* str_append_cstr(&instructions, tsprintf("#define run_battery_x ")); */
  /* for (s32 i = 1; i < max_size; i++) { */
  /*   str_append_cstr(&instructions, tsprintf("xtest%d ", i)); */
  /* } */
  /* str_append_cstr(&instructions, tsprintf("\n")); */
  da_append(&instructions, '\0');

  const char *inst_file = "instructions.h.out";
  if (!write_to_file(inst_file, instructions.items)) {
    plog(ERR, "Faild to write %s: %s", inst_file, strerror(errno));
    return KO;
  }
  da_free(&instructions);
  return OK;
}

EXPORT_RESULT_STRUCT_SIZE() { return sizeof(rob_result_t); }

EXPORT_RESULT_STRUCT_DIAGNOSTICS(rob_result_t *result) {
  usize max_size = 512;
  result->raw_readings_nop[0] = result->raw_readings_nop[1];
  result->raw_readings_xor[0] = result->raw_readings_xor[1];
  for (s32 i = 0; i < max_size; i++) {
    result->readings_nop[i] =
        (double)result->raw_readings_nop[i] / result->iterations;
    result->readings_xor[i] =
        (double)result->raw_readings_xor[i] / result->iterations;
  }

  double *filtered = malloc(max_size * sizeof(double));
  median_filter(max_size, result->readings_nop, filtered, 15);

  ssize rob_size = jump_welch_rel(max_size, filtered, 7, 60.f);

  free(filtered);

  /* ssize rob_size = */
  /* detect_sudden_jumps___(result->readings_nop, max_size, 16, 20.); */
  /* isize reg_size = */
  /*     detect_jumps_sliding(result->readings_xor, max_size, 16, 100.); */

  if (rob_size == -1) {
    plog(INFO, "Not found substatial jump");
    return KO;
  }

  plog(INFO, "Found rob %d", rob_size);
  result->rob_size = rob_size;
  /* result->register_file_size = reg_size; */
  return OK;
}
