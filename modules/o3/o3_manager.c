#include "../commands.h"
#include "o3_test.h"

#define IMPLEMENTATIONS
#include "../../libs/lbstd.h"
#include <stdlib.h>

#include "../cache/cache_test.h"

EXPORT_RESULT_SETUP(request_dependencies_t *dependencies) {
  cache_result_t *cache_r = dependencies[1];
  usize xor_amnt = 750;
  str instructions = {0};

  str_append_cstr(&instructions,
                  tsprintf("#define nodep_xor_count %d\n", xor_amnt));

  str_append_cstr(&instructions, "#ifdef TARGET_X86_64\n");
  str_append_cstr(&instructions, "#define nodep_xor \\\n");
  str_append_cstr(&instructions, "__asm__ __volatile__(\"\"\\\n");
  for (int i = 0; i < xor_amnt; i++) {
    str_append_cstr(&instructions,
                    tsprintf("\"xor $%u, %%%%rbx\\n\"\\\n", rand()));
  }
  str_append_cstr(&instructions, "::: \"rbx\");\n");
  str_append_cstr(&instructions, "#elif TARGET_RISCV\n");

  str_append_cstr(&instructions, "#define nodep_xor \\\n");
  str_append_cstr(&instructions, "__asm__ __volatile__(\"\"\\\n");
  for (int i = 0; i < xor_amnt; i++) {
    int imm = (rand() % 4096) - 2048;
    str_append_cstr(&instructions, tsprintf("\"xori t0, t0, %d\\n\"\\\n", imm));
  }
  str_append_cstr(&instructions, "::: \"t0\");\n");

  str_append_cstr(&instructions, "#endif \\\n");
  da_append(&instructions, '\0');

  const char *inst_file = "instructions.h.out";
  if (!write_to_file(inst_file, instructions.items)) {
    plog(ERR, "Faild to write %s: %s", inst_file, strerror(errno));
    return KO;
  }
  da_free(&instructions);

  return OK;
}

EXPORT_RESULT_STRUCT_SIZE() { return sizeof(o3_result_t); }

EXPORT_RESULT_STRUCT_DIAGNOSTICS(o3_result_t *result) {
  result->nodep_instruction_time = result->nodep_instruction_time_tot;
  result->nodep_instruction_time /= result->tries;
  result->nodep_instruction_time -= result->overhead;
  result->nodep_instruction_time /= result->number_of_instructions;

  result->uncached_access_time_with_instruction =
      result->uncached_access_time_with_instruction_tot;
  result->uncached_access_time_with_instruction /= result->tries;
  result->uncached_access_time_with_instruction -= result->overhead;

  plog(INFO, "instruction_time (single): %f", result->nodep_instruction_time);
  plog(INFO, "instruction_time (all): %f",
       result->nodep_instruction_time * result->number_of_instructions);
  plog(INFO, "uncached_time: %f", result->uncached_access_time);
  plog(INFO, "uncached_time_inst: %f",
       result->uncached_access_time_with_instruction);
  /* plog(INFO, "%f %f", result->uncached_access_time, */
  /*      result->nodep_instruction_time * result->number_of_instructions); */

  double in_order_expected_time =
      result->uncached_access_time +
      result->nodep_instruction_time * result->number_of_instructions;

  plog(INFO, "in order expected: %f", in_order_expected_time);

  bool has_o3 = !are_close(result->uncached_access_time_with_instruction,
                           in_order_expected_time, 10.f);

  if (has_o3) {
    plog(INFO, "Out of order detected");
    return OK;
  }

  plog(INFO, "Out of order not detected");
  return KO;
}
