#!/usr/bin/env bash
# The `integration` suite's select (.aidev/project.yaml): whether the step would
# run a script here, decided before AIDEV admits the suite into a docker slot.
# Exits 0 when the diff selects a script integration_script_needs can run and this
# host can run it (integration_preconditions: a docker daemon, the compose plugin
# and the time plan over HIVE_INTEGRATION_BUDGET), printing them; else 77, its last
# line the reason. It runs in AIDEV's worker, outside the checks' image: bash, git,
# coreutils/grep/sed and the docker CLI only; no build, no container, and nothing
# written under test-results/.
set -uo pipefail
cd "$(dirname "$0")/.." || exit 1
# shellcheck source=.aidev/integration.sh
. .aidev/integration.sh

skip() {  # REASON
    echo "$1"
    exit 77
}

[ "${HIVE_AIDEV_INTEGRATION:-0}" = 1 ] \
    || skip "off: HIVE_AIDEV_INTEGRATION is not 1 (.aidev/README.md, Integration scripts)"
base="$(diff_base)" \
    || skip "no diff base: none of AIDEV_BASE_REF, AIDEV_IMPLEMENT_BASE_SHA or a merge base with the integration branch resolves"
echo "diff base: $base"
selected="$(integration_selection "${base#*=}" | sort -u)"
[ -n "$selected" ] || skip "the diff from $base selects no tests/integration script"
runnable=()
for s in $selected; do
    integration_script_needs "$s" > /dev/null && runnable+=("$s")
done
[ "${#runnable[@]}" -gt 0 ] \
    || skip "the diff from $base selects only scripts needing fixtures only CI has: ${selected//$'\n'/ }"
now="$(date +%s)"
pre="$(integration_preconditions $(( $(integration_deadline "$now" "$now" "") - now )) "$(compiler_cache)" "${runnable[@]}")" \
    || { [ "$pre" = "${pre##*$'\n'}" ] || echo "${pre%$'\n'*}"; skip "skipped: ${pre##*$'\n'} (selected: ${runnable[*]})"; }
echo "$pre"
echo "selected: ${runnable[*]}"
