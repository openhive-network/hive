#!/bin/bash
# Run a Boost test binary's suites (chain_test by default) in parallel (same behavior as CI)
#
# Usage:
#   ./scripts/run_chain_tests_parallel.sh [path/to/test_binary] [parallelism] [units_file]
#
# Each line of units_file is one --run_test= argument run as its own process
# (e.g. "witness_tests/witness_basic_test" or "!witness_tests"), in file order.
# Without it, every top-level suite of the binary is a unit. Results are named
# after the binary (chain_test_results.xml, plugin_test_results.xml, ...) and
# written next to it.
#
# Examples:
#   ./scripts/run_chain_tests_parallel.sh                           # Auto-detect in build/
#   ./scripts/run_chain_tests_parallel.sh build/tests/unit/chain_test
#   ./scripts/run_chain_tests_parallel.sh build/tests/unit/chain_test 8
#   ./scripts/run_chain_tests_parallel.sh build/tests/unit/plugin_test 4 plugin_units.txt
#
# Requires GNU parallel: apt-get install parallel

set -e

# Kill a process and all its descendants
kill_tree() {
    local pid=$1
    local sig=${2:-TERM}
    # Get children before killing parent
    local children=$(pgrep -P $pid 2>/dev/null)
    for child in $children; do
        kill_tree $child $sig
    done
    kill -$sig $pid 2>/dev/null || true
}

# Handle Ctrl-C: kill all child processes
cleanup() {
    local exit_code=$?
    trap - INT TERM EXIT
    # Only show interrupt message and kill processes if actually interrupted
    if [ $exit_code -ne 0 ] || [ -n "$INTERRUPTED" ]; then
        echo ""
        echo "Interrupted, killing all test processes..."
        if [ -n "$PARALLEL_PID" ]; then
            kill_tree $PARALLEL_PID TERM
            sleep 0.5
            kill_tree $PARALLEL_PID KILL
        fi
        exit 130
    fi
}
interrupt_handler() {
    INTERRUPTED=1
    cleanup
}
trap interrupt_handler INT TERM
trap cleanup EXIT

CHAIN_TEST="${1:-}"
PARALLELISM="${2:-6}"
UNITS_FILE="${3:-}"

# Auto-detect chain_test if not provided
if [ -z "$CHAIN_TEST" ]; then
    for candidate in \
        "build/tests/unit/chain_test" \
        "build-testnet/tests/unit/chain_test" \
        "../build/tests/unit/chain_test"; do
        if [ -x "$candidate" ]; then
            CHAIN_TEST="$candidate"
            break
        fi
    done
fi

if [ -z "$CHAIN_TEST" ] || [ ! -x "$CHAIN_TEST" ]; then
    echo "Error: test binary not found or not executable"
    echo "Usage: $0 [path/to/test_binary] [parallelism] [units_file]"
    exit 1
fi

# Check for GNU parallel (not moreutils parallel)
if ! command -v parallel &> /dev/null; then
    echo "Error: GNU parallel not found."
    echo "Install with: apt-get install parallel"
    exit 1
fi

if ! parallel --version 2>&1 | grep -q "GNU parallel"; then
    echo "Error: Found 'parallel' but it's not GNU parallel (maybe moreutils?)."
    echo "Install GNU parallel: apt-get install parallel"
    exit 1
fi

if [ -n "$UNITS_FILE" ] && [ ! -r "$UNITS_FILE" ]; then
    echo "Error: units file $UNITS_FILE not readable"
    exit 1
fi

NAME=$(basename "$CHAIN_TEST")
echo "Using $NAME: $CHAIN_TEST"
echo "Parallelism: $PARALLELISM"

# Use the directory where the test binary resides for output
OUTPUT_DIR=$(dirname "$CHAIN_TEST")
CHAIN_TEST_TMPDIR=$(mktemp -d -t "${NAME}_tmp.XXXXXX")
echo "Output directory: $OUTPUT_DIR"
echo "Temp directory: $CHAIN_TEST_TMPDIR"

# Get list of test units
if [ -n "$UNITS_FILE" ]; then
    grep -v '^[[:space:]]*$' "$UNITS_FILE" > "$OUTPUT_DIR/${NAME}_list.txt" || true
else
    "$CHAIN_TEST" --list_content 2>&1 | \
        awk '/^[^ ]/ && /\*/ {suite=$1; gsub(/\*/, "", suite); print suite}' > "$OUTPUT_DIR/${NAME}_list.txt"
fi

NUM_SUITES=$(wc -l < "$OUTPUT_DIR/${NAME}_list.txt")
if [ "$NUM_SUITES" -eq 0 ]; then
    echo "Error: no test units to run"
    exit 1
fi
echo "Found $NUM_SUITES test units to run"
echo ""

# Run test suites in parallel (individual files go to temp dir)
CHAIN_TEST_ABS=$(readlink -f "$CHAIN_TEST")
OUTPUT_DIR_ABS=$(readlink -f "$OUTPUT_DIR")

# Disable exit-on-error for log collection
set +e

cd "$CHAIN_TEST_TMPDIR"
# Data directories are per job number: unit names may hold '/' or '!'.
parallel -j "$PARALLELISM" --joblog "${NAME}_parallel.log" --timeout 600 --termseq TERM,200,KILL,25 \
    "HIVE_TEMPDIR='$CHAIN_TEST_TMPDIR'/{#} '$CHAIN_TEST_ABS' --run_test={} --log_format=JUNIT --log_sink=${NAME}_{#}.xml --log_level=error > ${NAME}_{#}.log 2>&1; ret=\$?; echo Unit {} exited with code \$ret; exit \$ret" \
    < "$OUTPUT_DIR_ABS/${NAME}_list.txt" &
PARALLEL_PID=$!
wait $PARALLEL_PID || true

echo ""
echo "=== Parallel execution summary ==="
cat "${NAME}_parallel.log"

# Merge JUnit XML files (only numbered files to avoid matching ${NAME}_results.xml)
RESULTS="${NAME}_results.xml"
echo '<?xml version="1.0" encoding="UTF-8"?>' > "$RESULTS"
echo '<testsuites>' >> "$RESULTS"
for f in "${NAME}"_[0-9]*.xml; do
    if [ -f "$f" ]; then
        echo "Processing $f ($(wc -c < "$f") bytes)" >&2
        sed -n '/<testsuite/,/<\/testsuite>/p' "$f" >> "$RESULTS" 2>/dev/null || true
    fi
done
echo '</testsuites>' >> "$RESULTS"
echo "=== Merged XML size: $(wc -c < "$RESULTS") bytes ===" >&2

# Combine logs (only numbered files)
cat "${NAME}"_[0-9]*.log > "${NAME}.log" 2>/dev/null || true

# Copy results to output directory
cp "$RESULTS" "${NAME}.log" "${NAME}_parallel.log" "$OUTPUT_DIR_ABS/"

# Copy individual files to subdirectory for debugging (only numbered files)
mkdir -p "$OUTPUT_DIR_ABS/${NAME}_logs"
for f in "${NAME}"_[0-9]*.xml "${NAME}"_[0-9]*.log; do
    [ -f "$f" ] && cp "$f" "$OUTPUT_DIR_ABS/${NAME}_logs/"
done

cd "$OUTPUT_DIR_ABS"

# Check for failures
echo ""
FAILED_COUNT=$(awk 'NR>1 && $7!=0 {count++} END {print count+0}' "${NAME}_parallel.log")
PASSED_COUNT=$(awk 'NR>1 && $7==0 {count++} END {print count+0}' "${NAME}_parallel.log")

if [ "$FAILED_COUNT" -eq 0 ]; then
    echo "=== All $PASSED_COUNT test units passed ==="
    echo ""
    echo "Results in: $OUTPUT_DIR"
    echo "  - ${NAME}_results.xml (merged JUnit)"
    echo "  - ${NAME}.log (combined logs)"
    echo "  - ${NAME}_parallel.log (execution summary)"
    echo "  - ${NAME}_list.txt (test unit list)"
    exit 0
else
    echo "=== $FAILED_COUNT test units FAILED, $PASSED_COUNT passed ==="
    echo ""
    echo "Failed units:"
    awk 'NR>1 && $7!=0 {
        cmd=$0; sub(/.*--run_test=/, "", cmd); sub(/ .*/, "", cmd);
        print "  - " cmd " (exit code: " $7 ")"
    }' "${NAME}_parallel.log"
    echo ""
    echo "Results in: $OUTPUT_DIR"
    echo "Check individual logs: ${NAME}_logs/"
    exit 1
fi
