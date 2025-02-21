#! /bin/bash -x

set -xeuo pipefail

SOURCE_DATA_DIR=${1}
first_replay_block_limit=${2}
second_replay_block_limit=${3}

wait_for_instance() {
  local container_name=${1}
  local number_of_blocks_to_replay=${2}
  local LIMIT=300

  echo "Waiting for instance hosted by container: ${container_name}."

  echo "Querying for service status..."
  docker logs "${container_name}"
  # docker container exec -t "${container_name}" timeout $LIMIT bash -c "until curl --max-time 30  --data '{\"jsonrpc\": \"2.0\",\"method\": \"database_api.get_dynamic_global_properties\",\"id\": 1}' localhost:8091  | grep -o '\"head_block_number\":${number_of_blocks_to_replay},'; do sleep 3 ; done"
  docker container exec -t "${container_name}" timeout $LIMIT bash -c "until wget -O - -qnv --timeout=30  --post-data='{\"jsonrpc\": \"2.0\",\"method\": \"database_api.get_dynamic_global_properties\",\"id\": 1}' localhost:8091  | grep -o '\"head_block_number\":${number_of_blocks_to_replay},'; do sleep 3 ; done"
}

stop_instance() {
  local container_name="$1"
  docker stop "$container_name"
  docker wait "$container_name" || true
}

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
