#! /bin/bash -x

set -xeuo pipefail

instance_name=${1}
first_replay_block_limit=${2}
second_replay_block_limit=${3}

wait_for_instance() {
  local container_name=${1}
  local number_of_blocks_to_replay=${2}
  local LIMIT=300

  echo "Waiting for instance hosted by container: ${container_name}."

  CONTAINER_IP=$(docker inspect ${container_name} --format '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' || true)
  echo "container ip: $CONTAINER_IP"

  timeout $LIMIT bash <<EOF
  until [ ! -z "$CONTAINER_IP" ]
  do
    echo "Unable to get ${container_name} container IP.."
    docker inspect ${container_name} || true
    CONTAINER_IP=$(docker inspect ${container_name} --format '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' || true)
    sleep 3
  done
EOF

  echo "Querying for service status..."
  docker logs ${container_name}
  docker container exec -t ${container_name} timeout $LIMIT bash -c "until curl --max-time 30  --data '{\"jsonrpc\": \"2.0\",\"method\": \"database_api.get_dynamic_global_properties\",\"id\": 1}' localhost:8090  | grep -o '\"head_block_number\":${number_of_blocks_to_replay},'; do sleep 3 ; done"
  docker stop ${container_name}
}

SOURCE_DATA_DIR=$CI_PROJECT_DIR/data_generated_during_hive_replay
mkdir -p "$SOURCE_DATA_DIR"

echo "Create replay environment..."
"$SCRIPTS_PATH/ci-helpers/prepare_data_and_shm_dir.sh" --data-base-dir="$SOURCE_DATA_DIR" --block-log-source-dir="$BLOCK_LOG_SOURCE_DIR_5M" --config-ini-source="$CONFIG_INI_SOURCE"

echo "Start first replay: "
"$SCRIPTS_PATH"/run_hived_img.sh "$HIVED_IMAGE_NAME" --data-dir="$SOURCE_DATA_DIR"/datadir --name=$instance_name --docker-option="--network=bridge" --replay --stop-replay-at-block=1000000 --detach
wait_for_instance $instance_name $first_replay_block_limit
test "$(docker wait $instance_name)" = 0

echo "Start second replay: "
"$SCRIPTS_PATH"/run_hived_img.sh "$HIVED_IMAGE_NAME" --data-dir="$SOURCE_DATA_DIR"/datadir --name=$instance_name --docker-option="--network=bridge" --replay --stop-replay-at-block=1500000 --detach
wait_for_instance $instance_name $second_replay_block_limit
test "$(docker wait $instance_name)" = 0

echo "Test passed!"
