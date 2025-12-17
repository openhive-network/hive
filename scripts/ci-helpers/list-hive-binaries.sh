#!/bin/bash
# List available Hive testnet binaries and test block log images from successful pipelines
# Usage: ./scripts/ci-helpers/list-hive-binaries.sh [--recent N] [--branch BRANCH] [--block-logs]
#
# This helps you find values for QUICK_TEST mode variables.

set -euo pipefail

# Configuration
API_URL="${GITLAB_URL:-https://gitlab.syncad.com/api/v4}"
PROJECT_ID="${GITLAB_PROJECT_ID:-198}"  # hive/hive
REGISTRY="registry.gitlab.syncad.com/hive/hive"

# Defaults
RECENT=10
BRANCH=""
SHOW_BLOCK_LOGS=false

print_help() {
    cat <<EOF
Usage: $0 [OPTIONS]

List recent successful Hive pipelines with available testnet binaries and test block log images.

OPTIONS:
    --recent N      Show N most recent pipelines (default: 10)
    --branch NAME   Filter by branch name (default: all branches)
    --block-logs    Show available test block log images
    --help, -h      Show this help message

EXAMPLES:
    $0                          # List 10 most recent successful pipelines
    $0 --recent 20              # List 20 most recent
    $0 --branch develop         # Only show develop branch pipelines
    $0 --block-logs             # Show test block log images for test_tools_tests

QUICK_TEST USAGE:

  # Run only unit tests (chain_test, plugin_test) with auto-detected binaries:
  glab ci run -b my-branch --variables QUICK_TEST:true

  # Run specific tests (e.g., beekeeper tests):
  glab ci run -b my-branch --variables QUICK_TEST:true \\
    --variables QUICK_TEST_JOBS:run_examples_beekeeper_wasm

  # Run test_tools_tests (requires test block log image):
  glab ci run -b my-branch --variables QUICK_TEST:true \\
    --variables QUICK_TEST_JOBS:test_tools_tests \\
    --variables QUICK_TEST_BLOCK_LOG_IMAGE:<image>

SUPPORTED QUICK_TEST_JOBS:
  - run_examples_beekeeper_wasm  (also runs beekeeper_tsc_build)
  - test_beekeeper_wasm          (also runs beekeeper_tsc_build)
  - test-wax-spec-package        (also runs generate-wax-spec)
  - test_tools_tests             (requires QUICK_TEST_BLOCK_LOG_IMAGE)
EOF
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --recent)
            RECENT="$2"
            shift 2
            ;;
        --recent=*)
            RECENT="${1#*=}"
            shift
            ;;
        --branch)
            BRANCH="$2"
            shift 2
            ;;
        --branch=*)
            BRANCH="${1#*=}"
            shift
            ;;
        --block-logs)
            SHOW_BLOCK_LOGS=true
            shift
            ;;
        --help|-h)
            print_help
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            print_help
            exit 1
            ;;
    esac
done

# Check for required tools
if ! command -v jq &>/dev/null; then
    echo "ERROR: jq is required but not installed."
    echo "Install with: apt-get install jq  OR  brew install jq"
    exit 1
fi

# Check for authentication
TOKEN="${GITLAB_TOKEN:-}"
if [[ -z "$TOKEN" ]]; then
    echo "WARNING: GITLAB_TOKEN not set. API access may be limited."
    echo "Set it with: export GITLAB_TOKEN=<your-token>"
    echo ""
fi

# Build API URL
URL="${API_URL}/projects/${PROJECT_ID}/pipelines?status=success&per_page=${RECENT}"
if [[ -n "$BRANCH" ]]; then
    URL="${URL}&ref=${BRANCH}"
fi

echo "=== Recent Successful Hive Pipelines ==="
echo ""

# Fetch and display pipelines
if [[ -n "$TOKEN" ]]; then
    RESPONSE=$(curl -sf -H "PRIVATE-TOKEN: $TOKEN" "$URL" 2>/dev/null) || {
        echo "ERROR: Failed to fetch pipelines. Check your GITLAB_TOKEN."
        exit 1
    }
else
    RESPONSE=$(curl -sf "$URL" 2>/dev/null) || {
        echo "ERROR: Failed to fetch pipelines."
        exit 1
    }
fi

if [[ -z "$RESPONSE" ]] || [[ "$RESPONSE" == "[]" ]]; then
    echo "No successful pipelines found."
    if [[ -n "$BRANCH" ]]; then
        echo "Try without --branch filter, or check the branch name."
    fi
    exit 0
fi

# Parse and display
echo "COMMIT    BRANCH                          DATE                    PIPELINE"
echo "--------  ------------------------------  ----------------------  --------"

echo "$RESPONSE" | jq -r '.[] | "\(.sha[0:8])  \(.ref)  \(.created_at)  #\(.id)"' | while read -r line; do
    # Format: commit branch date pipeline_id
    commit=$(echo "$line" | awk '{print $1}')
    branch=$(echo "$line" | awk '{print $2}')
    date=$(echo "$line" | awk '{print $3}')
    pipeline=$(echo "$line" | awk '{print $4}')

    # Truncate/pad branch name for alignment
    branch_display=$(printf "%-30s" "${branch:0:30}")

    # Format date
    date_display=$(echo "$date" | sed 's/T/ /' | cut -c1-19)

    echo "$commit  $branch_display  $date_display  $pipeline"
done

echo ""
echo "=== Docker Image Check ==="
echo ""

# Check if images exist for first few commits
FIRST_COMMIT=$(echo "$RESPONSE" | jq -r '.[0].sha // empty' | head -c 8)
if [[ -n "$FIRST_COMMIT" ]]; then
    IMAGE="${REGISTRY}/testnet:${FIRST_COMMIT}"
    echo "Example testnet image: $IMAGE"
    echo ""
    echo "To verify an image exists, run:"
    echo "  docker manifest inspect ${REGISTRY}/testnet:<commit>"
fi

# Show test block log images if requested
if [[ "$SHOW_BLOCK_LOGS" == "true" ]]; then
    echo ""
    echo "=== Test Block Log Images ==="
    echo ""
    echo "Test block log images are used by test_tools_tests and related jobs."
    echo "Image format: ${REGISTRY}/testing-block-logs:combined-<checksum>"
    echo ""

    # Try to list available tags from registry
    if [[ -n "$TOKEN" ]]; then
        echo "Fetching available tags from registry..."
        REPO_ID=$(curl -sf -H "PRIVATE-TOKEN: $TOKEN" \
            "${API_URL}/projects/${PROJECT_ID}/registry/repositories" 2>/dev/null \
            | jq -r '.[] | select(.name == "testing-block-logs") | .id // empty') || true

        if [[ -n "$REPO_ID" ]]; then
            echo ""
            TAGS=$(curl -sf -H "PRIVATE-TOKEN: $TOKEN" \
                "${API_URL}/projects/${PROJECT_ID}/registry/repositories/${REPO_ID}/tags?per_page=5" 2>/dev/null \
                | jq -r '.[].name' 2>/dev/null) || true

            if [[ -n "$TAGS" ]]; then
                echo "Recent test block log image tags:"
                echo "$TAGS" | while read -r tag; do
                    echo "  ${REGISTRY}/testing-block-logs:${tag}"
                done
            else
                echo "No tags found or unable to fetch."
            fi
        else
            echo "Repository 'testing-block-logs' not found."
        fi
    else
        echo "Set GITLAB_TOKEN to list available image tags."
    fi

    echo ""
    echo "Usage with test_tools_tests:"
    echo "  glab ci run -b my-branch --variables QUICK_TEST:true \\"
    echo "    --variables QUICK_TEST_JOBS:test_tools_tests \\"
    echo "    --variables QUICK_TEST_BLOCK_LOG_IMAGE:${REGISTRY}/testing-block-logs:<tag>"
fi

echo ""
echo "=== Quick Test Usage ==="
echo ""
echo "# Run only unit tests (default):"
echo "glab ci run -b <your-branch> --variables QUICK_TEST:true"
echo ""
echo "# Run specific non-unit tests:"
echo "glab ci run -b <your-branch> --variables QUICK_TEST:true \\"
echo "  --variables QUICK_TEST_JOBS:run_examples_beekeeper_wasm"
echo ""
echo "# Run test_tools_tests (requires block log image):"
echo "glab ci run -b <your-branch> --variables QUICK_TEST:true \\"
echo "  --variables QUICK_TEST_JOBS:test_tools_tests \\"
echo "  --variables QUICK_TEST_BLOCK_LOG_IMAGE:${REGISTRY}/testing-block-logs:latest"
