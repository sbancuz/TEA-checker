#include "./pht_test.h"

#include "../commands.h"
#include "../tester.h"

#define IMPLEMENTATIONS
#include "../../libs/lbstd.h"

EXPORT_RESULT_SETUP(request_dependencies_t *dep) { return OK; }

EXPORT_RESULT_STRUCT_SIZE() { return sizeof(pht_result_t); }

EXPORT_RESULT_STRUCT_DIAGNOSTICS(pht_result_t *result) {
  plog(INFO, "overhead %f", result->overhead);

  double itime =
      (double)result->instruction_time_tot / result->tries - result->overhead;
  double correct =
      (double)result->branch_taken_time_tot / result->taken_counted;
  double miss =
      (double)result->branch_not_taken_time_tot / result->not_taken_counted;

  double penalty = miss - itime;

  double itimel = (double)result->instruction_time_tot_long / result->tries -
                  result->overhead;
  double correctl =
      (double)result->branch_taken_time_tot_long / result->taken_counted_long;
  double missl = (double)result->branch_not_taken_time_tot_long /
                 result->not_taken_counted_long;
  double penaltyl = missl - itimel;

  plog(INFO, "add25 time taken: %f", itime);
  plog(INFO, "branch taken: %f", correct);

  plog(INFO, "misprediction: %f", miss);
  plog(INFO, "penalty: %f", penalty);
  /* plog(INFO, "------------------------------------"); */
  /*  */
  /* plog(INFO, "add625 time taken l: %f", itimel); */
  /* plog(INFO, "branch taken l: %f", correctl); */
  /*  */
  /* plog(INFO, "misprediction l: %f", missl); */
  /* plog(INFO, "penaltyl: %f", penaltyl); */
  /*  */
  /* if (!are_close(penalty, penaltyl, 20.0)) { */
  /*   plog(WARN, */
  /*        "The 2 penalties are not close, further analysis has to be done!!");
   */
  /* } */

  if (are_close(correct, miss, 20.f)) {
    plog(INFO, "PHT not present");
    return KO;
  }

  result->penalty = (penalty + penaltyl) / 2;

  plog(INFO, "PHT present");
  return OK;
}
