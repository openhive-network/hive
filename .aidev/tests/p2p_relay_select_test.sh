#!/usr/bin/env bash
# Tests of .aidev/p2p-relay-select.sh in a scratch repository: which diffs
# select the `p2p-relay` step and what a skip says. Run from anywhere:
#
#   .aidev/tests/p2p_relay_select_test.sh
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
mkdir -p .aidev libraries/net libraries/plugins/p2p libraries/plugins/witness libraries/chain \
    libraries/protocol/include/hive/protocol tests/unit/plugin_tests doc
cp "$here/integration.sh" "$here/p2p-relay-select.sh" .aidev/
printf 'branches:\n  integration: "aidev/integration"\n' > .aidev/project.yaml
for f in libraries/net/node.cpp libraries/plugins/p2p/p2p_plugin.cpp libraries/plugins/witness/witness_plugin.cpp \
    libraries/chain/hive_evaluator.cpp tests/unit/plugin_tests/p2p_relay_tests.cpp doc/x.md; do
    echo base > "$f"
done
printf '#define HIVE_MAX_TRANSACTION_SIZE (1024*64)\n#define HIVE_MAX_URL_LENGTH 127\n' \
    > libraries/protocol/include/hive/protocol/config.hpp
printf 'void database::process_non_fast_confirm_transaction()\n{\n  auto trx_size_limit = 1;\n}\n\nvoid other()\n{\n  int x = 1;\n}\n' \
    > libraries/chain/database.cpp
git add -A && git commit -qm base
base="$(git rev-parse HEAD)"

run_select() {  # SELECT_ENV...: "EXIT|LAST LINE"
    local output rc
    output="$(env AIDEV_BASE_REF="$base" AIDEV_IMPLEMENT_BASE_SHA= "$@" .aidev/p2p-relay-select.sh 2>&1)"; rc=$?
    echo "$rc|${output##*$'\n'}"
}

result="$(run_select AIDEV_BASE_REF=nonexistent)"
expect "no diff base" "77|no_diff_base" "${result%%:*}"
echo docs >> doc/x.md
echo chain >> libraries/chain/hive_evaluator.cpp
echo witness >> libraries/plugins/witness/witness_plugin.cpp
sed -i 's/HIVE_MAX_URL_LENGTH 127/HIVE_MAX_URL_LENGTH 128/' libraries/protocol/include/hive/protocol/config.hpp
sed -i 's/int x = 1;/int x = 2;/' libraries/chain/database.cpp
result="$(run_select)"
expect "an evaluator, witness, docs and unrelated config/database diff is skipped" "77|not_selected" "${result%%:*}"
expect "the skip names the scope" "1" "$(grep -c 'libraries/net/ libraries/plugins/p2p/' <<< "$result")"
git checkout -q .
for f in libraries/net/node.cpp libraries/plugins/p2p/p2p_plugin.cpp tests/unit/plugin_tests/p2p_relay_tests.cpp; do
    echo changed >> "$f"
    expect "$f selects" "0|selected by: $f" "$(run_select)"
    git checkout -q .
done
sed -i 's/(1024\*64)/(1024*128)/' libraries/protocol/include/hive/protocol/config.hpp
expect "a size limit in config.hpp selects" "0|selected by: libraries/protocol/include/hive/protocol/config.hpp" "$(run_select)"
git checkout -q .
sed -i 's/trx_size_limit = 1;/trx_size_limit = 2;/' libraries/chain/database.cpp
expect "the transaction size check in database.cpp selects" "0|selected by: libraries/chain/database.cpp" "$(run_select)"
git checkout -q .
echo new > libraries/net/new.cpp
expect "an untracked net file selects" "0|selected by: libraries/net/new.cpp" "$(run_select)"
rm libraries/net/new.cpp
echo changed >> libraries/plugins/p2p/p2p_plugin.cpp
git commit -qam committed
expect "a committed change selects" "0|selected by: libraries/plugins/p2p/p2p_plugin.cpp" "$(run_select)"

[ "$failures" -eq 0 ] && echo "all passed" || { echo "$failures failed"; exit 1; }
