#!/bin/bash
# Run chain_test suites in parallel (same behavior as CI)
#
# Usage:
#   ./scripts/run_chain_tests_parallel.sh [path/to/chain_test] [parallelism]
#
# Examples:
#   ./scripts/run_chain_tests_parallel.sh                           # Auto-detect in build/
#   ./scripts/run_chain_tests_parallel.sh build/tests/unit/chain_test
#   ./scripts/run_chain_tests_parallel.sh build/tests/unit/chain_test 8
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
    trap - INT TERM EXIT
    echo ""
    echo "Interrupted, killing all test processes..."
    if [ -n "$PARALLEL_PID" ]; then
        kill_tree $PARALLEL_PID TERM
        sleep 0.5
        kill_tree $PARALLEL_PID KILL
    fi
    exit 130
}
trap cleanup INT TERM EXIT

CHAIN_TEST="${1:-}"
PARALLELISM="${2:-6}"

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
    echo "Error: chain_test not found or not executable"
    echo "Usage: $0 [path/to/chain_test] [parallelism]"
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

echo "Using chain_test: $CHAIN_TEST"
echo "Parallelism: $PARALLELISM"

# Create output directory
OUTPUT_DIR=$(mktemp -d -t chain_test_parallel.XXXXXX)
CHAIN_TEST_TMPDIR=$(mktemp -d -t chain_test_tmp.XXXXXX)
echo "Output directory: $OUTPUT_DIR"
echo "Temp directory: $CHAIN_TEST_TMPDIR"

# Get list of test suites
"$CHAIN_TEST" --list_content 2>&1 | \
    awk '/^[^ ]/ && /\*/ {suite=$1; gsub(/\*/, "", suite); print suite}' > "$OUTPUT_DIR/test_list.txt"

NUM_SUITES=$(wc -l < "$OUTPUT_DIR/test_list.txt")
echo "Found $NUM_SUITES test suites to run"
echo ""

# Run test suites in parallel
CHAIN_TEST_ABS=$(readlink -f "$CHAIN_TEST")
cd "$OUTPUT_DIR"

# Disable exit-on-error for log collection
set +e

parallel -j "$PARALLELISM" --joblog parallel.log --timeout 600 --termseq TERM,200,KILL,25 \
    "HIVE_TEMPDIR='$CHAIN_TEST_TMPDIR'/{} '$CHAIN_TEST_ABS' --run_test={} --log_format=JUNIT --log_sink=chain_test_{#}.xml --log_level=error > chain_test_{#}.log 2>&1; ret=\$?; echo 'Suite {} exited with code '\$ret; exit \$ret" \
    < test_list.txt &
PARALLEL_PID=$!
wait $PARALLEL_PID || true

echo ""
echo "=== Parallel execution summary ==="
cat parallel.log

# Merge JUnit XML files
echo '<?xml version="1.0" encoding="UTF-8"?>' > chain_test_results.xml
echo '<testsuites>' >> chain_test_results.xml
for f in chain_test_*.xml; do
    if [ -f "$f" ] && [ "$f" != "chain_test_results.xml" ]; then
        sed -n '/<testsuite/,/<\/testsuite>/p' "$f" >> chain_test_results.xml 2>/dev/null || true
    fi
done
echo '</testsuites>' >> chain_test_results.xml

# Combine logs
cat chain_test_*.log > chain_test.log 2>/dev/null || true

# Check for failures
echo ""
FAILED_COUNT=$(awk 'NR>1 && $7!=0 {count++} END {print count+0}' parallel.log)
PASSED_COUNT=$(awk 'NR>1 && $7==0 {count++} END {print count+0}' parallel.log)

if [ "$FAILED_COUNT" -eq 0 ]; then
    echo "=== All $PASSED_COUNT test suites passed ==="
    echo ""
    echo "Results in: $OUTPUT_DIR"
    echo "  - chain_test_results.xml (merged JUnit)"
    echo "  - chain_test.log (combined logs)"
    echo "  - parallel.log (execution summary)"
    exit 0
else
    echo "=== $FAILED_COUNT test suites FAILED, $PASSED_COUNT passed ==="
    echo ""
    echo "Failed suites:"
    awk 'NR>1 && $7!=0 {print "  - " $NF " (exit code: " $7 ")"}' parallel.log
    echo ""
    echo "Results in: $OUTPUT_DIR"
    echo "Check individual logs: chain_test_*.log"
    exit 1
fi
