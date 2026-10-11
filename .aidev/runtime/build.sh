#!/usr/bin/env bash
# Build (and with --push, publish) the test runtime image of .aidev/runtime/Dockerfile
# and print the digest-pinned reference to put in .aidev/project.yaml's
# `environment.image`. See .aidev/README.md.
#
# The tag is `aidev-<hash>`, the hash of the image input (the Dockerfile; every
# FROM is digest-pinned); the registry cleanup policy keeps `aidev-*` tags. When
# <repository>:aidev-<hash> is already in the registry nothing is built: the script
# prints that image's digest.
#
#   .aidev/runtime/build.sh           # registry digest, or build locally and print the local tag
#   .aidev/runtime/build.sh --push    # registry digest, or build + push and print repo@sha256:<digest>
#   .aidev/runtime/build.sh --tag     # print the input-hash tag only
set -euo pipefail

cd "$(dirname "$0")/../.."

REPOSITORY="${AIDEV_RUNTIME_REPOSITORY:-registry.gitlab.syncad.com/hive/hive/aidev-tests}"

# The Dockerfile copies nothing from the tree: it is the only input.
input_hash() {
    sha256sum .aidev/runtime/Dockerfile | sha256sum | cut -c1-16
}
TAG="aidev-$(input_hash)"

case "${1:-}" in
    --tag) echo "$TAG"; exit 0 ;;
    ""|--push) ;;
    *) echo "usage: $0 [--push|--tag]" >&2; exit 2 ;;
esac

# Digest of <repository>:<tag> in the registry, empty when the tag doesn't exist.
registry_digest() {
    docker buildx imagetools inspect "$REPOSITORY:$TAG" --format '{{json .Manifest}}' 2>/dev/null \
        | grep -o '"digest": *"sha256:[0-9a-f]*"' | head -1 | grep -o 'sha256:[0-9a-f]*' || true
}

digest="$(registry_digest)"
if [ -n "$digest" ]; then
    echo "$REPOSITORY:$TAG exists in the registry; not building" >&2
    echo "$REPOSITORY@$digest"
    exit 0
fi
echo "$REPOSITORY:$TAG is not in the registry; building it" >&2

# The Dockerfile copies nothing from the tree, so the context is empty.
context="$(mktemp -d)"
trap 'rm -rf "$context"' EXIT

# Optionally build through a pull-through registry cache near the build host, so FROM
# stays canonical and the build doesn't compete for the uplink. Set
# AIDEV_IMAGE_CACHE_BY_REGION to region=host:port pairs (comma-separated); a pair
# applies when its region is one of the labels of this host's FQDN, and a `*` pair
# applies to any other host. Unset (the default), the image is built straight from
# registry.gitlab.syncad.com. Prints the builder name, or nothing when no cache applies.
region_builder() {
    local cache_map="${AIDEV_IMAGE_CACHE_BY_REGION:-}"
    local region="*" label entry cache="" builder config
    [ -n "$cache_map" ] || return 0
    for label in $( (hostname -f 2>/dev/null || hostname) | tr 'A-Z.' 'a-z '); do
        for entry in ${cache_map//,/ }; do
            if [ "${entry%%=*}" = "$label" ]; then region="$label"; fi
        done
    done
    for entry in ${cache_map//,/ }; do
        if [ "${entry%%=*}" = "$region" ]; then cache="${entry#*=}"; fi
    done
    if [ -z "$cache" ]; then
        echo "no image cache applies to this host; building straight from registry.gitlab.syncad.com" >&2
        return
    fi
    builder="aidev-region-build-${cache//[.:]/-}"
    if ! docker buildx inspect "$builder" >/dev/null 2>&1; then
        config="$(mktemp)"
        printf '[registry."%s"]\n  mirrors = ["%s"]\n\n[registry."%s"]\n  http = true\n  insecure = true\n' \
            "registry.gitlab.syncad.com" "$cache" "$cache" >"$config"
        docker buildx create --name "$builder" --driver docker-container --config "$config" >/dev/null
        rm -f "$config"
    fi
    echo "$builder"
}
builder="$(region_builder)"

# The FROM is digest-pinned, so no --pull. No provenance, so the pushed digest is a
# plain image manifest.
build=(docker buildx build ${builder:+--builder "$builder"} --provenance=false -f .aidev/runtime/Dockerfile -t "$REPOSITORY:$TAG")
if [ "${1:-}" != "--push" ]; then
    "${build[@]}" --load "$context" >&2
    echo "$REPOSITORY:$TAG"
    exit 0
fi

"${build[@]}" --push --metadata-file "$context/metadata.json" "$context" >&2
digest="$(grep -o '"containerimage.digest": *"sha256:[0-9a-f]*"' "$context/metadata.json" | grep -o 'sha256:[0-9a-f]*')"
echo "$REPOSITORY@${digest:?no digest in buildx metadata}"
