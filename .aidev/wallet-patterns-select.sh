#!/usr/bin/env bash
# The `wallet_patterns` suite's select (.aidev/project.yaml): whether the diff
# reaches what the cli_wallet pattern tests check. Exits 0 when it changes a path
# below, printing the ones it changes; else 77, its last line the reason. It runs
# in AIDEV's worker, outside the checks' image: bash, git and coreutils/grep/sed
# only; no build, and nothing written under test-results/.
set -uo pipefail
cd "$(dirname "$0")/.." || exit 1
# shellcheck source=.aidev/integration.sh
. .aidev/integration.sh

# The wallet's API surface (libraries/protocol: the operation shapes it prints)
# and the patterns themselves.
wallet_patterns_paths=(
    libraries/wallet/
    programs/cli_wallet/
    libraries/protocol/
    tests/python/api_tests/python_patterns_tests/cli_wallet_tests/
)

skip() {  # REASON
    echo "$1"
    exit 77
}

base="$(diff_base)" \
    || skip "no diff base: none of AIDEV_BASE_REF, AIDEV_IMPLEMENT_BASE_SHA or a merge base with the integration branch resolves"
echo "diff base: $base"
changed="$({ git diff --name-only "${base#*=}" -- "${wallet_patterns_paths[@]}"
    git ls-files --others --exclude-standard -- "${wallet_patterns_paths[@]}"; } | sort -u)"
[ -n "$changed" ] || skip "the diff from $base changes none of ${wallet_patterns_paths[*]}"
echo "selected by: ${changed//$'\n'/ }"
