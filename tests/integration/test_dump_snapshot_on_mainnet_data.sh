#! /bin/bash -x

set -xeuo pipefail

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
SRC_DIR="$(realpath "${SCRIPTPATH}/../..")"
DATA_DIR="${SRC_DIR}/datadir/"
SNAPSHOT_NAME="test_snapshot"

# tmp
echo "######################### {$SCRIPTPATH}#########################"
mkdir -vp "${DATA_DIR}"

if [ "$#" -lt 2 ]; then
    echo "Usage: $0 <hived_image_tag> <data_dir> [options...]"
    exit 1
fi

HIVED_IMAGE_TAG=$1
DATA_DIR=$2
shift 2

echo "Snapshot will be stored in: ${DATA_DIR}/snapshot"
. $SRC_DIR/scripts/build_and_setup_exchange_instance.sh "$HIVED_IMAGE_TAG" --data-dir="$DATA_DIR" --dump-snapshot="$SNAPSHOT_NAME" --exit-at-block="100000" "$@"

find "$DATA_DIR" \
  -path "$DATA_DIR/snapshot" -prune -o \
  -name 'config.ini' -prune -o \
  -mindepth 1 -exec rm -rf {} +

cat $DATA_DIR/config.ini

ls -la "$DATA_DIR"
ls -la "$DATA_DIR/snapshot"

sleep 3
echo "Starting hived to load snapshot..."
sleep 1
"${SRC_DIR}"/build/programs/hived/hived --data-dir "$DATA_DIR" --load-snapshot "$SNAPSHOT_NAME" --plugin "database_api"
# . "${SRC_DIR}"/scripts/run_hived_img.sh "$HIVED_IMAGE_TAG" --name=hived-snapshot-instance --data-dir="$DATA_DIR" --load-snapshot "$SNAPSHOT_NAME"

# ./test_dump_snapshot_on_mainnet_data.sh 0d354898 /workspace/datadir --use-source-dir=/workspace/hive