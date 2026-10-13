#!/bin/bash
# Run a Boost test binary's suites (chain_test by default) in parallel (same behavior as CI)
#
# Usage:
#   ./scripts/run_chain_tests_parallel.sh [path/to/test_binary] [parallelism] [units_file] [unit_timeout]
#
# Each line of units_file is one --run_test= argument run as its own process
# (e.g. "witness_tests/witness_basic_test" or "!witness_tests"), in file order.
# Without it, every top-level suite of the binary is a unit. Results are named
# after the binary (chain_test_results.xml, plugin_test_results.xml, ...) and
# written next to it.
#
# parallelism is a number, or a file holding one that is read again each time
# a unit ends (GNU parallel's procfile), so a caller can change it mid-run.
#
# Each unit is killed after unit_timeout seconds (default 600). Under AIDEV
# with adaptive timeouts, AIDEV_TIMEOUTS_FILE may widen that per unit (never
# shorten it), and each unit that ends on its own is appended to
# AIDEV_TIMEOUTS_OBSERVATIONS; see .aidev/README.md.
#
# Examples:
#   ./scripts/run_chain_tests_parallel.sh                           # Auto-detect in build/
#   ./scripts/run_chain_tests_parallel.sh build/tests/unit/chain_test
#   ./scripts/run_chain_tests_parallel.sh build/tests/unit/chain_test 8
#   ./scripts/run_chain_tests_parallel.sh build/tests/unit/plugin_test 4 plugin_units.txt 900
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

# The timeout in milliseconds of action NAME, whose own default is DEFAULT_MS:
# its entry in AIDEV_TIMEOUTS_FILE, else DEFAULT_MS times the file's load scale,
# never less than DEFAULT_MS. Without a readable file, DEFAULT_MS.
aidev_timeout_ms() {  # NAME DEFAULT_MS
    if [ ! -r "${AIDEV_TIMEOUTS_FILE:-}" ]; then
        echo "$2"
        return
    fi
    python3 -c '
import json, math, sys
name, default = sys.argv[2], int(sys.argv[3])
try:
    with open(sys.argv[1]) as f:
        doc = json.load(f)
    action = doc.get("actions", {}).get(name)
    ms = action["timeout_ms"] if action else default * doc.get("scale", 1)
    print(max(default, math.ceil(ms)))
except (OSError, ValueError, TypeError, KeyError, AttributeError):
    print(default)
' "$AIDEV_TIMEOUTS_FILE" "$1" "$2" 2>/dev/null || echo "$2"
}

# Run one unit under its own timeout. A unit that ended on its own (passed, or
# failed its assertions) is an observation; a killed one is not, since its
# duration says only that it took longer.
run_unit() {  # SEQ UNIT TIMEOUT_SECONDS
    local start ret took_ms
    start=$(date +%s%3N)
    HIVE_TEMPDIR="$CHAIN_TEST_TMPDIR/$1" timeout -k 30 "$3" "$CHAIN_TEST_ABS" --run_test="$2" \
        --log_format=JUNIT --log_sink="${NAME}_$1.xml" --log_level=error > "${NAME}_$1.log" 2>&1
    ret=$?
    took_ms=$(( $(date +%s%3N) - start ))
    if [ "$ret" -eq 124 ] || [ "$ret" -eq 137 ]; then
        echo "Unit $2 killed at its timeout of $3s"
    else
        echo "Unit $2 exited with code $ret"
    fi
    # 124-127: killed at the timeout or never ran; 128-199: killed by a signal.
    # Boost.Test's own failures are 200 and 201.
    case "$ret" in
        12[4-9]|1[3-9][0-9]) ;;
        *) [ -z "${AIDEV_TIMEOUTS_OBSERVATIONS:-}" ] ||
            printf '{"action": "%s:%s", "duration_ms": %d, "default_ms": %d}\n' \
                "$NAME" "$2" "$took_ms" "$UNIT_DEFAULT_MS" >> "$AIDEV_TIMEOUTS_OBSERVATIONS" ;;
    esac
    return "$ret"
}

CHAIN_TEST="${1:-}"
PARALLELISM="${2:-6}"
UNITS_FILE="${3:-}"
UNIT_DEFAULT_MS=$(( ${4:-600} * 1000 ))

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

[ -r "${AIDEV_TIMEOUTS_FILE:-}" ] && echo "Adaptive timeouts: $AIDEV_TIMEOUTS_FILE"
while IFS= read -r unit; do
    ms=$(aidev_timeout_ms "$NAME:$unit" "$UNIT_DEFAULT_MS")
    printf '%s\t%s\n' "$unit" $(( (ms + 999) / 1000 ))
done < "$OUTPUT_DIR_ABS/${NAME}_list.txt" > "$CHAIN_TEST_TMPDIR/${NAME}_timeouts.tsv"

# Disable exit-on-error for log collection
set +e

cd "$CHAIN_TEST_TMPDIR"
# Data directories are per job number: unit names may hold '/' or '!'.
export -f run_unit
export CHAIN_TEST_TMPDIR CHAIN_TEST_ABS NAME UNIT_DEFAULT_MS
parallel -j "$PARALLELISM" --joblog "${NAME}_parallel.log" --termseq TERM,200,KILL,25 --colsep '\t' \
    run_unit {#} {1} {2} < "$CHAIN_TEST_TMPDIR/${NAME}_timeouts.tsv" &
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
        unit=$11; gsub(/[\\\047]/, "", unit);
        print "  - " unit " (exit code: " $7 ")"
    }' "${NAME}_parallel.log"
    echo ""
    echo "Results in: $OUTPUT_DIR"
    echo "Check individual logs: ${NAME}_logs/"
    exit 1
fi
