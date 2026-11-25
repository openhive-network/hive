#!/usr/bin/env bash
# Tests of .aidev/junit_cases.py's failure text over stub step logs. Run from
# anywhere:
#
#   .aidev/tests/junit_cases_test.sh
set -uo pipefail
here="$(cd "$(dirname "$0")/.." && pwd)"
scratch="$(mktemp -d)"
trap 'rm -rf "$scratch"' EXIT
failures=0

expect() {  # NAME EXPECTED ACTUAL
    if [ "$2" = "$3" ]; then echo "ok   $1"; else echo "FAIL $1: expected [$2], got [$3]"; failures=$((failures + 1)); fi
}

junit() {  # CASES: the report junit_cases.py writes for CASES
    python3 "$here/junit_cases.py" "$scratch/junit.xml" quick "$1" && cat "$scratch/junit.xml"
}

failure_text() {  # the first <failure> element's text, unescaped
    python3 -c '
import sys, xml.etree.ElementTree as ET
f = ET.parse(sys.argv[1]).getroot().find("testcase/failure")
print(f.get("message")); print(f.text or "", end="")' "$scratch/junit.xml"
}

# A build that fails while other jobs still run: their progress lines follow the
# failed command's output, and the stats sccache_hits prints end the log.
build_log="$scratch/build.log"
{
    for i in $(seq 1 200); do echo "[$i/900] Building CXX object libraries/x/CMakeFiles/x.dir/unit$i.cpp.o"; done
    echo "FAILED: libraries/chain/CMakeFiles/hive_chain.dir/database.cpp.o "
    printf '/usr/bin/sccache /usr/bin/c++ %s -c /home/hived/source/libraries/chain/database.cpp\n' "$(printf -- '-I/x/include%.0s ' $(seq 1 100))"
    echo "/home/hived/source/libraries/chain/database.cpp:42:7: error: expected ';' before '}' token"
    echo "   42 |   int x"
    echo "compilation terminated due to -Wfatal-errors."
    for i in $(seq 201 260); do echo "[$i/900] Building CXX object libraries/y/CMakeFiles/y.dir/unit$i.cpp.o"; done
    echo "ninja: build stopped: subcommand failed."
    for i in $(seq 1 40); do echo "Compile requests                 $i"; done
} > "$build_log"
printf 'case\tbuild\tfail\t9\texit 1\t%s\n' "$build_log" > "$scratch/build.tsv"
junit "$scratch/build.tsv" > /dev/null
text="$(failure_text)"
expect "build: the message names the failed object" \
    "exit 1; FAILED: libraries/chain/CMakeFiles/hive_chain.dir/database.cpp.o" "$(head -1 <<< "$text")"
expect "build: the text starts with the FAILED: line" \
    "FAILED: libraries/chain/CMakeFiles/hive_chain.dir/database.cpp.o" "$(sed -n 2p <<< "$text")"
expect "build: the compiler error follows it" \
    "/home/hived/source/libraries/chain/database.cpp:42:7: error: expected ';' before '}' token" "$(sed -n 3p <<< "$text")"
expect "build: the error is within the text's first 500 characters" 1 \
    "$(tail -n +2 <<< "$text" | head -c 500 | grep -c "database.cpp:42:7: error")"
expect "build: the next job's progress line ends the block" 0 \
    "$(sed -n '/^FAILED/,/^--- last/p' <<< "$text" | grep -c '^\[201/900\]')"
expect "build: the command, shortened" 1 "$(grep -c '^command: /usr/bin/sccache .*\.\.\.$' <<< "$text")"
expect "build: the log's tail ends the text" "Compile requests                 40" "$(tail -1 <<< "$text")"
expect "--cause prints the same body" "$(tail -n +2 <<< "$text")" "$(python3 "$here/junit_cases.py" --cause "$build_log")"

# A failing test step: no FAILED: line, the tail alone, as before.
test_log="$scratch/chain-smoke.log"
{
    for i in $(seq 1 100); do echo "line $i"; done
    echo "basic_tests.cpp(12): error: in \"basic_tests/parse_size\": check a == b has failed"
    echo "*** 1 failure is detected in the test module \"Master Test Suite\""
} > "$test_log"
printf 'case\tchain-smoke\tfail\t3\texit 201\t%s\n' "$test_log" > "$scratch/test.tsv"
junit "$scratch/test.tsv" > /dev/null
text="$(failure_text)"
expect "test: the message unchanged" "exit 201" "$(head -1 <<< "$text")"
expect "test: the text is the log's last 80 lines" "$(tail -80 "$test_log")" "$(tail -n +2 <<< "$text")"

printf 'case\tbuild\tpass\t5\t\ncase\tchain-smoke\tskip\t0\tnot_selected: x\n' > "$scratch/pass.tsv"
junit "$scratch/pass.tsv" > /dev/null
expect "passing and skipped cases carry no failure" "0 1" \
    "$(grep -c '<failure' "$scratch/junit.xml") $(grep -c '<skipped message="not_selected: x"/>' "$scratch/junit.xml")"

[ "$failures" -eq 0 ] && echo "all passed" || { echo "$failures failed"; exit 1; }
