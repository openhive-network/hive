#!/bin/bash

set -euo pipefail

# Download block_log parts from Synology NAS using fbdownload endpoint
# Usage: download_block_log_parts.sh <base_url> <path> <last_part> <output_dir>

if [ "$#" -ne 4 ]; then
    echo "Usage: $0 <base_url> <path> <last_part> <output_dir>"
    echo ""
    echo "Arguments:"
    echo "  base_url    Base URL for fbdownload endpoint (e.g., https://termit.pl.syncad.com/fbdownload)"
    echo "  path        Path parameter for fbdownload (e.g., /files)"
    echo "  last_part   Last part number to download (e.g., 5 for parts 0001-0005)"
    echo "  output_dir  Directory to save downloaded files"
    echo ""
    echo "Example:"
    echo "  $0 https://termit.pl.syncad.com/fbdownload /files 5 ./block_logs"
    exit 1
fi

BASE_URL=$1
PATH_PARAM=$2
LAST_PART=$3
OUTPUT_DIR=$4

# Create output directory if it doesn't exist
mkdir -p "$OUTPUT_DIR"

echo "=== Block Log Download Configuration ==="
echo "Base URL: $BASE_URL"
echo "Path: $PATH_PARAM"
echo "Downloading parts: 0001-$(printf '%04d' $LAST_PART)"
echo "Output directory: $OUTPUT_DIR"
echo "========================================"

# Download needed parts (0001 to LAST_PART)
for i in $(seq 1 $LAST_PART); do
  PART=$(printf "%04d" $i)
  PART_FILE="block_log_part.$PART"
  ARTIFACTS_FILE="$PART_FILE.artifacts"

  echo "Downloading $PART_FILE..."
  curl -k -f -o "$OUTPUT_DIR/$PART_FILE" \
    "$BASE_URL/$PART_FILE?path=$PATH_PARAM" || {
      echo "ERROR: Failed to download $PART_FILE"
      exit 1
    }

  echo "Downloading $ARTIFACTS_FILE..."
  curl -k -f -o "$OUTPUT_DIR/$ARTIFACTS_FILE" \
    "$BASE_URL/$ARTIFACTS_FILE?path=$PATH_PARAM" || {
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
