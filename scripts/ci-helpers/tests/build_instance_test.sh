#!/usr/bin/env bash
# Tests of the buildx cache arguments build_instance.sh passes, over a docker stub
# that records its argv instead of building. Run from anywhere:
#
#   scripts/ci-helpers/tests/build_instance_test.sh
set -uo pipefail
here="$(cd "$(dirname "$0")/.." && pwd)"
srcroot="$(cd "$here/../.." && pwd)"
scratch="$(mktemp -d)"
trap 'rm -rf "$scratch"' EXIT
failures=0

expect() {  # NAME EXPECTED ACTUAL
    if [ "$2" = "$3" ]; then echo "ok   $1"; else echo "FAIL $1: expected [$2], got [$3]"; failures=$((failures + 1)); fi
}

mkdir -p "$scratch/bin"
cat > "$scratch/bin/docker" <<'STUB'
#!/usr/bin/env bash
[ "$1 $2" = "buildx version" ] && exit 0
printf '%s\n' "$@" > "$DOCKER_ARGV"
STUB
chmod +x "$scratch/bin/docker"

registry=registry.example.com/hive/hive
cache_to="--cache-to=type=registry,ref=$registry/cache:testnet-build,mode=max,compression=zstd,force-compression=false"
cache_from="--cache-from=type=registry,ref=$registry/cache:testnet-build
--cache-from=type=registry,ref=$registry/cache:testnet-build-instance"

build() {  # [ENV=VALUE]... -- [OPTION]...: the buildx argv, one per line
    local env=()
    while [ "$1" != -- ]; do env+=("$1"); shift; done
    shift
    rm -f "$scratch/argv"
    (cd "$scratch" && env -u CI -u USE_BUILDX -u POSTGRES_VERSION PATH="$scratch/bin:$PATH" \
        DOCKER_ARGV="$scratch/argv" "${env[@]}" \
        "$here/build_instance.sh" tag "$srcroot" "$registry" --network-type=testnet "$@") > /dev/null 2>&1
    echo "exit $?"
    cat "$scratch/argv" 2>/dev/null
}

local_argv="$(build --)"
expect "CI unset: builds" "exit 0" "$(head -1 <<< "$local_argv")"
expect "CI unset: buildx build" "buildx build" "$(sed -n '2,3p' <<< "$local_argv" | paste -sd ' ')"
expect "CI unset: no --cache-to" "" "$(grep -- '^--cache-to' <<< "$local_argv")"
expect "CI unset: --cache-from the registry cache" "$cache_from" "$(grep -- '^--cache-from' <<< "$local_argv")"

ci_argv="$(build CI=true --)"
expect "CI=true: builds" "exit 0" "$(head -1 <<< "$ci_argv")"
expect "CI=true: --cache-to the registry cache" "$cache_to" "$(grep -- '^--cache-to' <<< "$ci_argv")"
expect "CI=true: --cache-from the registry cache" "$cache_from" "$(grep -- '^--cache-from' <<< "$ci_argv")"
expect "CI=true: otherwise the same argv" "$(grep -v -- '^--cache-to' <<< "$ci_argv" | grep -v '^BUILD_TIME=')" \
    "$(grep -v '^BUILD_TIME=' <<< "$local_argv")"

optin_argv="$(build -- --cache-to-registry)"
expect "--cache-to-registry without CI: --cache-to the registry cache" "$cache_to" "$(grep -- '^--cache-to' <<< "$optin_argv")"

expect "--help documents --cache-to-registry" 1 \
    "$(cd "$scratch" && "$here/build_instance.sh" --help 2>&1 | grep -c -- '--cache-to-registry  ')"

[ "$failures" -eq 0 ] && echo "all passed" || { echo "$failures failed"; exit 1; }
