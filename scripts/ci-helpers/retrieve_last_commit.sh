#!/bin/bash
#
# retrieve_last_commit.sh - Find the last commit that changed specified files
#
# DEPRECATED: This script is a wrapper for backward compatibility.
# New code should use find-last-source-commit.sh from common-ci-configuration.
#
# Usage: retrieve_last_commit.sh <directory> <pattern1> [pattern2] ...
#

set -euo pipefail

DIR="${1:-.}"
shift
PATTERNS=("$@")

# Try to use the new script from common-ci-configuration
CI_SCRIPTS_DIR="${CI_SCRIPTS_DIR:-/tmp/common-ci-scripts}"
NEW_SCRIPT="$CI_SCRIPTS_DIR/find-last-source-commit.sh"

if [[ -x "$NEW_SCRIPT" ]]; then
    # Use new script (already fetched by get_image4submodule.sh)
    exec "$NEW_SCRIPT" --dir="$DIR" --full --quiet "${PATTERNS[@]}"
else
    # Fall back to original implementation
    pushd "$DIR" >/dev/null 2>&1 || exit 1
    COMMIT=$(git log --pretty=format:"%H" -n 1 -- "${PATTERNS[@]}" 2>/dev/null | head -1)
    popd >/dev/null 2>&1 || exit 1
    echo "$COMMIT"
fi
