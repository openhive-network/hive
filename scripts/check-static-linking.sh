#!/bin/bash
# Check that a binary is statically linked except for expected glibc dependencies
#
# Usage: check-static-linking.sh <binary> [binary2] [binary3] ...
#
# Expected dynamic dependencies (glibc-based systems):
#   - linux-vdso.so.1      - kernel virtual DSO (not a real file)
#   - ld-linux-x86-64.so.2 - ELF dynamic linker
#   - libc.so.6            - glibc core
#   - libm.so.6            - glibc math library
#   - librt.so.1           - glibc real-time extensions (stub in glibc 2.34+)
#   - libdl.so.2           - glibc dynamic loading (stub in glibc 2.34+)
#   - libpthread.so.0      - glibc threads (stub in glibc 2.34+)
#
# Any other dynamic dependency will cause the test to fail.
#
# Exit codes:
#   0 - All binaries pass (only expected dependencies)
#   1 - One or more binaries have unexpected dynamic dependencies
#   2 - Usage error (no binaries specified, binary not found, etc.)

set -euo pipefail

# ANSI colors (disabled if not a terminal)
if [[ -t 1 ]]; then
    RED='\033[0;31m'
    GREEN='\033[0;32m'
    YELLOW='\033[0;33m'
    NC='\033[0m' # No Color
else
    RED=''
    GREEN=''
    YELLOW=''
    NC=''
fi

# Expected dynamic libraries (glibc and kernel)
# These are the only acceptable dynamic dependencies
EXPECTED_LIBS=(
    "linux-vdso.so"
    "ld-linux-x86-64.so"
    "ld-linux-aarch64.so"  # ARM64 support
    "libc.so"
    "libm.so"
    "librt.so"
    "libdl.so"
    "libpthread.so"
)

usage() {
    echo "Usage: $0 <binary> [binary2] [binary3] ..."
    echo ""
    echo "Check that binaries are statically linked except for expected glibc dependencies."
    echo ""
    echo "Options:"
    echo "  -h, --help     Show this help message"
    echo "  -v, --verbose  Show all dependencies, not just unexpected ones"
    echo ""
    echo "Examples:"
    echo "  $0 /path/to/hived"
    echo "  $0 /path/to/hived /path/to/cli_wallet /path/to/compress_block_log"
}

verbose=false
binaries=()

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            usage
            exit 0
            ;;
        -v|--verbose)
            verbose=true
            shift
            ;;
        -*)
            echo -e "${RED}Error: Unknown option: $1${NC}" >&2
            usage >&2
            exit 2
            ;;
        *)
            binaries+=("$1")
            shift
            ;;
    esac
done

if [[ ${#binaries[@]} -eq 0 ]]; then
    echo -e "${RED}Error: No binaries specified${NC}" >&2
    usage >&2
    exit 2
fi

# Check if ldd is available
if ! command -v ldd &> /dev/null; then
    echo -e "${RED}Error: ldd command not found${NC}" >&2
    exit 2
fi

# Function to check if a library is expected
is_expected_lib() {
    local lib="$1"
    for expected in "${EXPECTED_LIBS[@]}"; do
        if [[ "$lib" == *"$expected"* ]]; then
            return 0
        fi
    done
    return 1
}

# Function to check a single binary
check_binary() {
    local binary="$1"
    local has_unexpected=false
    local unexpected_libs=()
    local all_libs=()

    if [[ ! -f "$binary" ]]; then
        echo -e "${RED}Error: Binary not found: $binary${NC}" >&2
        return 2
    fi

    if [[ ! -x "$binary" ]]; then
        echo -e "${RED}Error: Binary not executable: $binary${NC}" >&2
        return 2
    fi

    # Get dynamic dependencies
    # ldd output format: "libname.so.X => /path/to/lib (0x...)" or "libname.so.X (0x...)"
    local ldd_output
    if ! ldd_output=$(ldd "$binary" 2>&1); then
        # Check if it's a statically linked binary (ldd returns "not a dynamic executable")
        if [[ "$ldd_output" == *"not a dynamic executable"* ]] || [[ "$ldd_output" == *"statically linked"* ]]; then
            echo -e "${GREEN}✓ $binary: fully statically linked${NC}"
            return 0
        fi
        echo -e "${RED}Error running ldd on $binary: $ldd_output${NC}" >&2
        return 2
    fi

    # Parse ldd output
    while IFS= read -r line; do
        # Skip empty lines
        [[ -z "$line" ]] && continue

        # Extract library name (first field, possibly after whitespace)
        local lib
        lib=$(echo "$line" | awk '{print $1}')

        # Skip if empty
        [[ -z "$lib" ]] && continue

        all_libs+=("$lib")

        if ! is_expected_lib "$lib"; then
            has_unexpected=true
            unexpected_libs+=("$lib")
        fi
    done <<< "$ldd_output"

    # Report results
    local binary_name
    binary_name=$(basename "$binary")

    if $verbose; then
        echo -e "${YELLOW}Dependencies for $binary_name:${NC}"
        for lib in "${all_libs[@]}"; do
            if is_expected_lib "$lib"; then
                echo -e "  ${GREEN}✓ $lib${NC} (expected glibc/kernel)"
            else
                echo -e "  ${RED}✗ $lib${NC} (UNEXPECTED)"
            fi
        done
        echo ""
    fi

    if $has_unexpected; then
        echo -e "${RED}✗ $binary_name: has unexpected dynamic dependencies:${NC}"
        for lib in "${unexpected_libs[@]}"; do
            echo -e "  ${RED}- $lib${NC}"
        done
        return 1
    else
        echo -e "${GREEN}✓ $binary_name: correctly linked (${#all_libs[@]} expected dependencies)${NC}"
        return 0
    fi
}

# Main
exit_code=0
total=${#binaries[@]}
passed=0
failed=0

echo "Checking static linking for $total binary(ies)..."
echo ""

for binary in "${binaries[@]}"; do
    if check_binary "$binary"; then
        ((passed++)) || true
    else
        ((failed++)) || true
        exit_code=1
    fi
done

echo ""
echo "========================================="
if [[ $failed -eq 0 ]]; then
    echo -e "${GREEN}All $total binary(ies) passed static linking check${NC}"
else
    echo -e "${RED}$failed of $total binary(ies) failed static linking check${NC}"
fi
echo "========================================="

exit $exit_code
