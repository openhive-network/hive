#! /bin/bash

SCRIPT_DIR="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 || exit 1; pwd -P )"
SRC_DIR="$SCRIPT_DIR/.."


echo "$SRC_DIR"
ls -la "$SRC_DIR"
pwd

if [ "$#" -lt 3 ]; then
    echo "Usage: $0 <hived_image> <data_dir> <exit_at_block> <snapshot_name: default> [options...]"
    exit 1
fi

HIVED_IMAGE_NAME=$1
DATA_DIR=$2
EXIT_AT_BLOCK=$3
shift 3

echo "Using HIVED image: $HIVED_IMAGE_NAME"
echo "Using HIVED data dir: $DATA_DIR"
echo "Snapshot will be dumped at block: $EXIT_AT_BLOCK"

echo "#### ARGS2: $@"

"$SCRIPT_DIR/run_instance.sh" \
    "$HIVED_IMAGE_NAME" \
    --data-dir="$DATA_DIR" \
    --dump-snapshot="snapshot" \
    --exit-at-block="$EXIT_AT_BLOCK" \
    --replay \
    --block-log-split=9999 \
    --detach

echo "Snapshot dumped to: ${DATA_DIR}/snapshot/${SNAPSHOT_NAME}, storage state at block ${EXIT_AT_BLOCK}"
