#!/bin/bash
# Quick Test Setup Script
# Downloads pre-built testnet binaries from a previous successful pipeline
# Usage: Called by quick_test_setup CI job when QUICK_TEST=true
#
# Environment Variables:
#   QUICK_TEST_BINARY_COMMIT - Specific commit to use (optional, auto-detected if not set)
#   QUICK_TEST_JOBS          - Additional jobs to run (comma-separated, e.g., "test_tools_tests")
#   QUICK_TEST_BLOCK_LOG_IMAGE - Test block log image for test_tools_tests (required if running that job)

set -euo pipefail

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"

# JSON parsing helper (works without jq using Python)
json_get_sha() {
    python3 -c "import sys,json;d=json.load(sys.stdin);print(d[0]['sha'][:8] if d else '')" 2>/dev/null || true
}

# Configuration
REGISTRY="${CI_REGISTRY_IMAGE:-registry.gitlab.syncad.com/hive/hive}"
BINARY_PATH="${CI_PROJECT_DIR:-$(pwd)}/hived-testnet-binaries"
API_URL="${CI_API_V4_URL:-https://gitlab.syncad.com/api/v4}"
PROJECT_ID="${CI_PROJECT_ID:-198}"
BRANCH="${CI_COMMIT_REF_NAME:-develop}"

echo "=== Quick Test Setup ==="
echo "Registry: $REGISTRY"
echo "Binary path: $BINARY_PATH"
echo "Branch: $BRANCH"
if [[ -n "${QUICK_TEST_JOBS:-}" ]]; then
    echo "Additional jobs: $QUICK_TEST_JOBS"
fi
if [[ -n "${QUICK_TEST_BLOCK_LOG_IMAGE:-}" ]]; then
    echo "Block log image: $QUICK_TEST_BLOCK_LOG_IMAGE"
fi

# Determine which commit to use for binaries
resolve_binary_commit() {
    local commit=""

    if [[ -n "${QUICK_TEST_BINARY_COMMIT:-}" ]]; then
        commit="$QUICK_TEST_BINARY_COMMIT"
        echo "Using provided commit: $commit" >&2
    else
        echo "Auto-detecting last successful pipeline..." >&2

        # Helper to fetch pipeline SHA
        fetch_sha() {
            local ref="$1"
            local response
            local url="${API_URL}/projects/${PROJECT_ID}/pipelines?ref=${ref}&status=success&per_page=5"
            echo "DEBUG: Fetching $url" >&2
            # Try with JOB-TOKEN header (for CI jobs) and fallback to PRIVATE-TOKEN (for personal tokens)
            response=$(curl -sf -H "JOB-TOKEN: ${CI_JOB_TOKEN:-}" "$url" 2>/dev/null) || \
                response=$(curl -sf -H "PRIVATE-TOKEN: ${GITLAB_TOKEN:-}" "$url" 2>/dev/null) || true
            if [[ -n "$response" ]]; then
                echo "DEBUG: Got response (${#response} chars)" >&2
                echo "$response" | json_get_sha
            else
                echo "DEBUG: No response from API" >&2
            fi
        }

        # Try current branch first
        commit=$(fetch_sha "$BRANCH")
        if [[ -n "$commit" ]]; then
            echo "Found successful pipeline on branch '$BRANCH': $commit" >&2
        else
            # Fallback to develop branch
            echo "No successful pipeline on '$BRANCH', checking develop..." >&2
            commit=$(fetch_sha "develop")

            if [[ -n "$commit" ]]; then
                echo "Found successful pipeline on develop: $commit" >&2
            else
                # Last resort: master
                echo "No successful pipeline on develop, checking master..." >&2
                commit=$(fetch_sha "master")
            fi
        fi
    fi

    if [[ -z "$commit" ]]; then
        echo "ERROR: Could not find any successful pipeline with binaries."
        echo "Please specify QUICK_TEST_BINARY_COMMIT manually, or run a full pipeline first."
        echo ""
        echo "To find available commits, run: ./scripts/ci-helpers/list-hive-binaries.sh"
        exit 1
    fi

    echo "$commit"
}

# Extract binaries from Docker image
extract_binaries() {
    local commit="$1"
    local image="${REGISTRY}/testnet:${commit}"

    echo ""
    echo "=== Retrieving binaries from $image ==="

    # Login to registry if credentials available
    local registry_pass="${REGISTRY_PASS:-${HIVED_CI_IMGBUILDER_PASSWORD:-}}"
    local registry_user="${REGISTRY_USER:-${HIVED_CI_IMGBUILDER_USER:-gitlab-ci-token}}"
    if [[ -n "$registry_pass" ]]; then
        echo "$registry_pass" | \
            docker login -u "$registry_user" "${REGISTRY%%/*}" --password-stdin 2>/dev/null || true
    fi

    # Check if image exists
    if ! docker manifest inspect "$image" >/dev/null 2>&1; then
        echo "ERROR: Image not found: $image"
        echo ""
        echo "This could mean:"
        echo "  1. The commit $commit never had a successful testnet build"
        echo "  2. The image was cleaned up from the registry"
        echo ""
        echo "Try a different commit with: QUICK_TEST_BINARY_COMMIT=<sha>"
        echo "To list available commits: ./scripts/ci-helpers/list-hive-binaries.sh"
        exit 1
    fi

    # Create output directory
    mkdir -p "$BINARY_PATH"

    # Use the existing export script
    if [[ -x "$SCRIPTPATH/export-data-from-docker-image.sh" ]]; then
        "$SCRIPTPATH/export-data-from-docker-image.sh" "$image" "$BINARY_PATH"
    else
        # Fallback: use docker build to extract
        echo "Extracting binaries using docker build..."
        docker build -o "$BINARY_PATH" - <<EOF
FROM scratch
COPY --from=${image} /home/hived/bin/ /
EOF
    fi

    # Verify binaries were extracted
    if [[ ! -f "$BINARY_PATH/hived" ]] && [[ ! -f "$BINARY_PATH/chain_test" ]]; then
        echo "ERROR: Failed to extract binaries - no executables found in $BINARY_PATH"
        ls -la "$BINARY_PATH" || true
        exit 1
    fi

    # Make binaries executable
    chmod -R a+rx "$BINARY_PATH"

    echo ""
    echo "=== Binaries extracted successfully ==="
    ls -la "$BINARY_PATH"
}

# Main
BINARY_COMMIT=$(resolve_binary_commit)

# Export resolved commit for downstream jobs
if [[ -n "${CI_PROJECT_DIR:-}" ]]; then
    echo "QUICK_TEST_RESOLVED_COMMIT=$BINARY_COMMIT" > "${CI_PROJECT_DIR}/quick_test.env"
    echo ""
    echo "Resolved commit exported to quick_test.env: $BINARY_COMMIT"
fi

extract_binaries "$BINARY_COMMIT"

echo ""
echo "=== Quick Test Setup Complete ==="
echo "Binary commit: $BINARY_COMMIT"
echo "Binary path: $BINARY_PATH"
echo ""
echo "Ready to run unit tests with pre-built binaries."
