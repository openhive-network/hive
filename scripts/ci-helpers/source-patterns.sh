#!/bin/sh
#
# source-patterns.sh - Patterns for files that trigger hived image rebuilds
#
# These are SOURCE CODE patterns only - changes to these files require
# rebuilding hived binaries. Used by:
#   - get_image4submodule.sh (find last source commit for image lookup)
#   - skip_rules.yml (detect source changes in CI)
#   - downstream repos like clive (find hived images via URL fetch)
#
# NOT included here (handled separately in skip_rules.yml):
#   - Test patterns (tests/) - trigger test runs, not rebuilds
#   - Doc patterns (*.md, doc/) - skip CI entirely
#   - CI patterns (scripts/ci-helpers/) - trigger full pipeline
#
# Usage:
#   source-patterns.sh           # comma-separated (for git log pathspecs)
#   source-patterns.sh --regex   # regex pattern (for grep -E)

PATTERNS="libraries/
programs/
docker/
contrib/
Dockerfile
cmake
CMakeLists.txt
.gitmodules
scripts/build.sh"

case "${1:-}" in
    --regex)
        # Output as regex for grep -E: ^(libraries/|programs/|...)
        printf '^(%s)' "$(echo "$PATTERNS" | tr '\n' '|' | sed 's/|$//')"
        ;;
    *)
        # Output as comma-separated for git pathspecs
        echo "$PATTERNS" | tr '\n' ',' | sed 's/,$//'
        ;;
esac
