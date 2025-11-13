#! /bin/bash

SCRIPT_DIR="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 || exit 1; pwd -P )"

if [ "$#" -lt 3 ]; then
    echo "Usage: $0 <hived_image> <data_dir> <exit_at_block> <snapshot_name: optional> [options...]"
    exit 1
fi

HIVED_IMAGE_TAG=$1
DATA_DIR=$2
EXIT_AT_BLOCK=$3

if ! [[ "$EXIT_AT_BLOCK" =~ ^[0-9]+$ ]]; then
    echo "Error: exit_at_block must be a number"
    exit 1
fi

shift 3

if [[ $# -gt 0 && $1 != -* ]]; then
    SNAPSHOT_NAME=$1
    shift 1
else
    SNAPSHOT_NAME="snapshot"
fi

echo "Using HIVED image: $HIVED_IMAGE_TAG"
echo "Using HIVED data dir: $DATA_DIR"
echo "Snapshot will be dumped at block: $EXIT_AT_BLOCK"

"$SCRIPT_DIR/run_instance.sh" \
    "$HIVED_IMAGE_TAG" \
    --data-dir="$DATA_DIR" \
    --dump-snapshot="$SNAPSHOT_NAME" \
    --exit-at-block="$EXIT_AT_BLOCK" \
    "$@"

echo "Snapshot dumped to: ${DATA_DIR}/snapshot/${SNAPSHOT_NAME}, storage state at block ${EXIT_AT_BLOCK}"
