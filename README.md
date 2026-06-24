# TEA checker

### build
```bash
cc -o build build.c && ./bulid -h
```

All the needed analyzer dependencies are in the `flake.nix` file.

```
matplotlib
pygccxml
castxml
```

### run

This runs a single test, if `<test_name> = all` it runs every test in `./modules`.

```bash
./orchestrator -t TARGET_X86_64 -r RUNNER_USER -s <out_file> <test_name>
```

To perform analysis use `analyzer.py`, make sure all the required packages are installed

```bash
usage: analyzer.py [-h] [--plot PLOT] [--pp [PP]] [--export [EXPORT]] run

Plotter for tests

positional arguments:
  run                   Run file or directory

options:
  -h, --help            show this help message and exit
  --plot PLOT           Name of the module to plot
  --pp, --pretty-print [PP]
                        Pretty-print a module (or all if no name given)
  --export [EXPORT]     Export to CSV
```

Note: no arguments or export don't work for runs that are not `all`, plot only works if the module haeder file specifies it.
