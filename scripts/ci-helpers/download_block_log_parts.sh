#!/bin/bash

set -euo pipefail

# Download block_log parts from remote HTTP server
# Usage: download_block_log_parts.sh <base_url> <last_part> <output_dir>

# Detect max available part number on remote server using binary search
detect_max_part() {
    local base_url=$1
    local low=1
    local high=10000  # Upper limit for binary search
    local max_found=0

    echo "Detecting max available part on remote server..." >&2

    # First check if part 0001 exists
    if ! curl -f -s -I "$base_url/block_log_part.0001" >/dev/null 2>&1; then
        echo "ERROR: block_log_part.0001 not found on remote server" >&2
        return 1
    fi

    # Binary search to find the last available part
    while [ $low -le $high ]; do
        local mid=$(( (low + high) / 2 ))
        local part_file=$(printf "block_log_part.%04d" $mid)

        # Check if file exists (curl -f fails on 404)
        if curl -f -s -I "$base_url/$part_file" >/dev/null 2>&1; then
            max_found=$mid
            low=$((mid + 1))
        else
            high=$((mid - 1))
        fi
    done

    if [ $max_found -eq 0 ]; then
        echo "ERROR: Could not detect any available parts" >&2
        return 1
    fi

    echo $max_found
}

if [ "$#" -ne 3 ]; then
    echo "Usage: $0 <base_url> <last_part> <output_dir>"
    echo ""
    echo "Arguments:"
    echo "  base_url    Base URL to block_log directory (e.g., http://termit.pl.syncad.com:3000/block_log_sources/mainnet_full)"
    echo "  last_part   Last part number to download, or 'auto' to detect max available"
    echo "              (e.g., 5 for parts 0001-0005, 'auto' for all available)"
    echo "  output_dir  Directory to save downloaded files"
    echo ""
    echo "Examples:"
    echo "  $0 http://termit.pl.syncad.com:3000/block_log_sources/mainnet_full 5 ./block_logs"
    echo "  $0 http://termit.pl.syncad.com:3000/block_log_sources/mainnet_full auto ./block_logs"
    exit 1
fi

BASE_URL=$1
LAST_PART_INPUT=$2
OUTPUT_DIR=$3

# Detect max available parts
MAX_AVAILABLE=$(detect_max_part "$BASE_URL")
if [ $? -ne 0 ]; then
    echo "ERROR: Failed to detect available parts"
    exit 1
fi

echo "Max available parts on remote: $MAX_AVAILABLE (block_log_part.$(printf '%04d' $MAX_AVAILABLE))"

# Determine LAST_PART to use
if [ "$LAST_PART_INPUT" = "auto" ]; then
    LAST_PART=$MAX_AVAILABLE
    echo "Using auto-detected max: $LAST_PART parts"
else
    LAST_PART=$LAST_PART_INPUT

    # Validate requested part doesn't exceed available
    if [ "$LAST_PART" -gt "$MAX_AVAILABLE" ]; then
        echo "ERROR: Requested LAST_PART ($LAST_PART) exceeds available parts ($MAX_AVAILABLE)"
        exit 1
    fi

    echo "Using requested: $LAST_PART parts (max available: $MAX_AVAILABLE)"
fi

# Create output directory if it doesn't exist
mkdir -p "$OUTPUT_DIR"

echo "=== Block Log Download Configuration ==="
echo "Base URL: $BASE_URL"
echo "Downloading parts: 0001-$(printf '%04d' $LAST_PART)"
echo "Output directory: $OUTPUT_DIR"
echo "========================================"

# Download needed parts (0001 to LAST_PART)
for i in $(seq 1 $LAST_PART); do
  PART=$(printf "%04d" $i)
  PART_FILE="block_log_part.$PART"
  ARTIFACTS_FILE="$PART_FILE.artifacts"

  echo "Downloading $PART_FILE..."
  curl -f -o "$OUTPUT_DIR/$PART_FILE" "$BASE_URL/$PART_FILE" || {
    echo "ERROR: Failed to download $PART_FILE"
    exit 1
  }

  echo "Downloading $ARTIFACTS_FILE..."
  curl -f -o "$OUTPUT_DIR/$ARTIFACTS_FILE" "$BASE_URL/$ARTIFACTS_FILE" || {
    echo "  (artifacts not found for $PART_FILE, skipping)"
  }
done

DOWNLOADED_FILES=$(ls "$OUTPUT_DIR" | wc -l)
echo ""
echo "Download complete!"
echo "Downloaded $DOWNLOADED_FILES files to $OUTPUT_DIR"
echo ""
echo "Files:"
ls -lh "$OUTPUT_DIR"
