#include "unistd.h"
#include <getopt.h>
#include <stdio.h>

#define IMPLEMENTATIONS
#include "libs/lbstd.h"

int main(int argc, char *argv[]) {
  go_rebuild_urself(argc, argv);
  const char *program_name = argv[0];

  int opt;

  bool bear_flag = false;
  bool run_flag = false;
  bool yes_flag = false;

  const char *help_templ =
      "help %s:\n"
      "\t--run/-r\t\tRun the compiled program\n"
      "\t--bear/-b\t\tCreate compile_commands.json using `bear`\n"
      "\t--yes/-y\t\tAnswer yes to all questions\n"
      "\t--help/-h\t\tPrint this help\n";

  while (1) {
    int option_index = 0;
    static struct option long_options[] = {{"run", no_argument, 0, 'r'},
                                           {"bear", no_argument, 0, 'b'},
                                           {"Yes", no_argument, 0, 'y'},
                                           {"help", no_argument, 0, 'h'},
                                           {0, 0, 0, 0}};

    opt = getopt_long(argc, argv, "+rbyh", long_options, &option_index);
    if (opt == -1)
      break;

    switch (opt) {
    case 'r':
      run_flag = true;
      break;
    case 'y':
      yes_flag = true;
      break;
    case 'b':
      bear_flag = true;
      break;

    case 'h':
      printf(help_templ, program_name);
      exit(0);
      break;

    default:
      plog(ERR, "unrecognized flag: %c", opt);
      printf(help_templ, program_name);
      exit(1);
    }
  }

  int dashdash_index = -1;
  for (int i = 0; i < argc; i++) {
    if (strcmp(argv[i], "--") == 0) {
      dashdash_index = i;
      break;
    }
  }
  int pass_args = -1;
  if (dashdash_index != -1) {
    pass_args = argc - dashdash_index - 1;
  }

  cmd_t c = {};
  if (needs_rebuild("orchestrator", (const char *[]){"orchestrator.c"}, 1)) {
    cmd_append(&c, __BEAR "cc", "-o", "orchestrator", "orchestrator.c",
               SANITIZERS);

    if (bear_flag)
      cmd_append(&c, "-DBEAR=1");

    cmd_t csuid = {};
    cmd_append(&csuid, "/bin/sh", "-c",
               "sudo -E /bin/sh -c 'chown root:root ./orchestrator && "
               "chmod u+s "
               "./orchestrator'");

    if (!yes_flag) {
      plog(INFO,
           "This program will compile `orchestrator` to a SUID binary. Is it "
           "okay? [y/N]: ");
      char choice = getchar();
      if (choice != 'y') {
        exit(0);
      }
    }

    if (!cmd_run_reset(&c)) {
      plog(ERR, "Compilation failed!");
      return 1;
    }

    if (!cmd_run_reset(&csuid)) {
      return 1;
    }
  }

  if (run_flag) {
    cmd_append(&c, "./orchestrator");
    if (pass_args > 0) {
      da_append_many(&c.c, &argv[dashdash_index + 1], pass_args);
    }
    cmd_run_reset(&c);
  }

  return 0;
}
