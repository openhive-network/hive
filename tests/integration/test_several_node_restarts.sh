#! /bin/bash -x

set -xeuo pipefail

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
source "$SCRIPTPATH/conftest.sh"

SOURCE_DATA_DIR=${1}
first_replay_block_limit=${2}
second_replay_block_limit=${3}

mkdir -p "$SOURCE_DATA_DIR"

echo "Create replay environment..."
"$SCRIPTS_PATH/ci-helpers/prepare_data_and_shm_dir.sh" --data-base-dir="$SOURCE_DATA_DIR" --block-log-source-dir="$BLOCK_LOG_SOURCE_DIR" --config-ini-source="$CONFIG_INI_SOURCE"

echo "Start first replay: "
"$SCRIPTS_PATH"/run_hived_img.sh "$HIVED_IMAGE_NAME" --data-dir="$SOURCE_DATA_DIR"/datadir --name=first-replay-container --docker-option="--network=bridge" --replay --stop-at-block="$first_replay_block_limit" --detach --shared-file-size=1G
wait_for_instance first-replay-container "$first_replay_block_limit"
stop_instance "first-replay-container"

echo "Start second replay: "
"$SCRIPTS_PATH"/run_hived_img.sh "$HIVED_IMAGE_NAME" --data-dir="$SOURCE_DATA_DIR"/datadir --name=second-replay-container --docker-option="--network=bridge" --replay --stop-at-block="$second_replay_block_limit" --detach --shared-file-size=1G
wait_for_instance second-replay-container "$second_replay_block_limit"
stop_instance "second-replay-container"

echo "Test passed!"
