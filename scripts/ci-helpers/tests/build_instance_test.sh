#!/usr/bin/env bash
# Tests of the buildx cache arguments build_instance.sh passes, with docker stubbed
# by a recorder of its argv, so no build runs. Run from anywhere:
#
#   scripts/ci-helpers/tests/build_instance_test.sh
set -uo pipefail
here="$(cd "$(dirname "$0")/.." && pwd)"
src="$(cd "$here/../.." && pwd)"
scratch="$(mktemp -d)"
trap 'rm -rf "$scratch"' EXIT
failures=0
registry="registry.example.com/hive/hive"
cache_from_build="--cache-from=type=registry,ref=$registry/cache:mainnet-build"
cache_from_instance="--cache-from=type=registry,ref=$registry/cache:mainnet-build-instance"
cache_to="--cache-to=type=registry,ref=$registry/cache:mainnet-build,mode=max,compression=zstd,force-compression=false"

mkdir "$scratch/bin"
cat > "$scratch/bin/docker" <<'STUB'
#!/usr/bin/env bash
[ "$1 ${2:-}" = "buildx version" ] && exit 0
printf '%s\n' "$@" > "$DOCKER_ARGV"
STUB
chmod +x "$scratch/bin/docker"

fail() { echo "FAIL $*"; failures=$((failures + 1)); }

build() {  # NAME CI(""=unset) [ARG]...: runs the script, leaving docker's argv in $scratch/NAME/argv
    local name="$1" ci="$2"; shift 2
    local ci_env=(-u CI)
    [ -n "$ci" ] && ci_env=(CI="$ci")
    mkdir "$scratch/$name"
    (cd "$scratch/$name" && env "${ci_env[@]}" DOCKER_ARGV="$scratch/$name/argv" PATH="$scratch/bin:$PATH" \
        "$here/build_instance.sh" test-tag "$src" "$registry" "$@" > output 2>&1) \
        || { fail "$name: build_instance.sh failed:"; tail -20 "$scratch/$name/output"; }
    [ -s "$scratch/$name/argv" ] || { fail "$name: docker buildx was not run"; : > "$scratch/$name/argv"; }
}

expect_arg() {  # NAME BUILD ARG
    if grep -qxF -- "$3" "$scratch/$2/argv"; then echo "ok   $1"; else fail "$1: [$3] not in argv"; fi
}

expect_cache_to_count() {  # NAME BUILD COUNT
    local n
    n="$(grep -c -- '^--cache-to' "$scratch/$2/argv")"
    if [ "$n" = "$3" ]; then echo "ok   $1"; else fail "$1: expected $3 --cache-to, got $n"; fi
}

build local ""
expect_arg "local build reads the build cache" local "$cache_from_build"
expect_arg "local build reads the instance cache" local "$cache_from_instance"
expect_cache_to_count "local build writes no cache" local 0
expect_arg "local build tags the image" local "$registry:test-tag"
expect_arg "local build loads the image" local "--load"

build ci true
expect_arg "CI build reads the build cache" ci "$cache_from_build"
expect_arg "CI build reads the instance cache" ci "$cache_from_instance"
expect_arg "CI build writes the cache" ci "$cache_to"
expect_cache_to_count "CI build writes one cache" ci 1
expect_arg "CI build tags the image" ci "$registry:test-tag"

build optin "" --cache-to-registry
expect_arg "--cache-to-registry writes the cache without CI" optin "$cache_to"
expect_cache_to_count "--cache-to-registry writes one cache" optin 1

if [ "$failures" -eq 0 ]; then echo "all passed"; else echo "$failures failed"; exit 1; fi
