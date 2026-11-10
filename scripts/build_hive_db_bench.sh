#!/usr/bin/env bash
# Builds hive_db_bench, RocksDB's db_bench able to open hived's RocksDB databases, which
# scripts/rocksdb_trace_replay.py runs (doc/devs/rocksdb-trace-replay.md).
#
#   scripts/build_hive_db_bench.sh [<build dir>]
#
# It gets a build directory of its own (default: build-db-bench in the source tree), because
# HIVE_BUILD_DB_BENCH builds RocksDB from source instead of using a preinstalled one, and fetches
# gflags (v2.2.2, from GitHub) unless CMake finds it installed. Prints the binary's path.
set -euo pipefail

src="$(cd "$(dirname "$0")/.." && pwd)"
bld="${1:-$src/build-db-bench}"

cmake -S "$src" -B "$bld" -GNinja -DCMAKE_BUILD_TYPE=Release -DHIVE_BUILD_DB_BENCH=ON >&2
cmake --build "$bld" --target hive_db_bench >&2
echo "$bld/programs/util/hive_db_bench"
