#!/usr/bin/env bash
# Tests of .aidev/wallet-patterns-select.sh in a scratch repository: which diffs
# select the `wallet_patterns` suite and what a skip says. Run from anywhere:
#
#   .aidev/tests/wallet_patterns_select_test.sh
set -uo pipefail
here="$(cd "$(dirname "$0")/.." && pwd)"
scratch="$(mktemp -d)"
trap 'rm -rf "$scratch"' EXIT
failures=0

expect() {  # NAME EXPECTED ACTUAL
    if [ "$2" = "$3" ]; then echo "ok   $1"; else echo "FAIL $1: expected [$2], got [$3]"; failures=$((failures + 1)); fi
}

cd "$scratch" || exit 1
git init -q && git config user.email t@t && git config user.name t
mkdir -p .aidev libraries/wallet libraries/protocol libraries/chain programs/cli_wallet doc \
    tests/python/api_tests/python_patterns_tests/cli_wallet_tests
cp "$here/integration.sh" "$here/wallet-patterns-select.sh" .aidev/
printf 'branches:\n  integration: "aidev/integration"\n' > .aidev/project.yaml
for f in libraries/wallet/wallet.cpp libraries/protocol/operations.cpp libraries/chain/database.cpp \
    programs/cli_wallet/main.cpp doc/x.md tests/python/api_tests/python_patterns_tests/cli_wallet_tests/p.json; do
    echo base > "$f"
done
git add -A && git commit -qm base
base="$(git rev-parse HEAD)"

run_select() {  # SELECT_ENV...: "EXIT|LAST LINE"
    local output rc
    output="$(env AIDEV_BASE_REF="$base" AIDEV_IMPLEMENT_BASE_SHA= "$@" .aidev/wallet-patterns-select.sh 2>&1)"; rc=$?
    echo "$rc|${output##*$'\n'}"
}

result="$(run_select AIDEV_BASE_REF=nonexistent)"
expect "no diff base" "77|no diff base" "${result%%:*}"
echo docs >> doc/x.md
echo chain >> libraries/chain/database.cpp
expect "a docs and chain diff is skipped" \
    "77|the diff from AIDEV_BASE_REF=$base changes none of libraries/wallet/ programs/cli_wallet/ libraries/protocol/ tests/python/api_tests/python_patterns_tests/cli_wallet_tests/" \
    "$(run_select)"
git checkout -q .
for f in libraries/wallet/wallet.cpp programs/cli_wallet/main.cpp libraries/protocol/operations.cpp \
    tests/python/api_tests/python_patterns_tests/cli_wallet_tests/p.json; do
    echo changed >> "$f"
    expect "$f selects" "0|selected by: $f" "$(run_select)"
    git checkout -q .
done
echo new > libraries/wallet/new.cpp
expect "an untracked wallet file selects" "0|selected by: libraries/wallet/new.cpp" "$(run_select)"
rm libraries/wallet/new.cpp
echo changed >> libraries/wallet/wallet.cpp
git commit -qam committed
expect "a committed change selects" "0|selected by: libraries/wallet/wallet.cpp" "$(run_select)"

[ "$failures" -eq 0 ] && echo "all passed" || { echo "$failures failed"; exit 1; }
