#!/usr/bin/env bash
# Whether the diff reaches what plugin_test's p2p_relay_tests checks (the
# `p2p-relay` step of .aidev/run-checks.sh): the p2p and network code, and the
# transaction and block size limits. Exits 0 when it does, printing the files
# that select it; else 77, its last line the reason. bash, git and
# coreutils/grep/sed only; no build, and nothing written under test-results/.
set -uo pipefail
cd "$(dirname "$0")/.." || exit 1
# shellcheck source=.aidev/integration.sh
. .aidev/integration.sh

# A path prefix and the extended regex one of its changed lines (or a changed
# hunk's header, which names the function) has to match; `.` for any change.
p2p_relay_table() {
    cat <<'EOF'
libraries/net/                                      .
libraries/plugins/p2p/                              .
tests/unit/plugin_tests/p2p_relay_tests.cpp         .
libraries/protocol/include/hive/protocol/config.hpp HIVE_MAX_BLOCK_SIZE|HIVE_MAX_TRANSACTION_SIZE|HIVE_MIN_BLOCK_SIZE_LIMIT
libraries/chain/database.cpp                        trx_size|get_maximum_block_size
EOF
}

skip() {  # REASON
    echo "$1"
    exit 77
}

base="$(diff_base)" \
    || skip "no_diff_base: none of AIDEV_BASE_REF, AIDEV_IMPLEMENT_BASE_SHA or a merge base with the integration branch resolves"
echo "diff base: $base"
changed="$({ git diff --name-only "${base#*=}" --; git ls-files --others --exclude-standard; } | sort -u)"
selected=() scope=()
while read -r path lines; do
    if [ "$lines" = . ]; then scope+=("$path"); else scope+=("$path ($lines)"); fi
    while IFS= read -r f; do
        case "$f" in "$path"*) ;; *) continue ;; esac
        if [ "$lines" = . ] || { git diff -U0 "${base#*=}" -- "$f" | grep -E '^(@@|[-+])'
                git ls-files --others --exclude-standard -- "$f" | xargs -r cat; } | grep -qE "$lines"; then
            selected+=("$f")
        fi
    done <<< "$changed"
done < <(p2p_relay_table)
[ "${#selected[@]}" -gt 0 ] || skip "not_selected: the diff from $base changes none of ${scope[*]}"
echo "selected by: ${selected[*]}"
