#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
echo $SCRIPT_DIR

SRC_DIR="$SCRIPT_DIR/../proofs/cpus/chipyard/"
DST_DIR="$SCRIPT_DIR/../proofs/chipyard/generators/chipyard/src/main/scala/config"

mkdir -p "$DST_DIR"

# Hardlink all regular files (non-recursive)
find "$SRC_DIR" -maxdepth 1 -type f -print0 | while IFS= read -r -d '' file; do
    ln -f "$file" "$DST_DIR/$(basename "$file")"
done

pushd $SCRIPT_DIR/../proofs/chipyard/ > /dev/null
source ./env.sh

# Build simulators
pushd ./sims/verilator > /dev/null
# Predefined list of configs

# make clean

configs=("CustomBoomV3Config")
# Loop through each config and run the make command
for cfg in "${configs[@]}"; do
    echo "Building with config: $cfg"
    VERILATOR_THREADS=8 NUMACTL=1 CXXFLAGS="-O3 -march=native" make -j$(nproc) CONFIG=$cfg
done

popd > /dev/null
# Fix tests
pushd ./tests > /dev/null

# Create/clear test.c
echo '' > test.c

# Add add_executable(test test.c) only if it doesn't already exist
grep -qxF 'add_executable(test test.c)' CMakeLists.txt || sed -i '/^add_executable(symmetric symmetric\.c)/a\
add_executable(test test.c)
' CMakeLists.txt

# Add add_dump_target(test) only if it doesn't already exist
grep -qxF 'add_dump_target(test)' CMakeLists.txt || sed -i '/^add_dump_target(symmetric)/a\
add_dump_target(test)
' CMakeLists.txt

# Run cmake with the include path relative to the chipyard root
cmake -DCMAKE_C_FLAGS="-I$SCRIPT_DIR/../include/ -DTARGET_RISCV -DRUNNER_SIMULATION" .

popd > /dev/null
popd > /dev/null
