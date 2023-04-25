#! /bin/bash -x

set -xeuo pipefail

wait_for_instance() {
  local container_name=${1}
  local LIMIT=300

  echo "Waiting for instance hosted by container: ${container_name}."

  docker inspect ${container_name} || true

  CONTAINER_IP=$(docker inspect ${container_name} --format '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' || true)

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

  sleep $LIMIT

  docker container exec -t ${container_name} timeout $LIMIT bash -c "until curl --max-time 30  --data '{\"jsonrpc\": \"2.0\",\"method\": \"database_api.get_dynamic_global_properties\",\"id\": 1}' ${CONTAINER_IP}:8090; do sleep 3 ; done"
}

#SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
#BASE_DIRECTORY=/$(echo "$SCRIPTPATH" | cut -d "/" -f2)
SOURCE_DATA_DIR=$CI_PROJECT_DIR/data_generated_during_hive_replay
mkdir -p "$SOURCE_DATA_DIR"
ls -lah
echo "ENV:"
env
echo "FNI ENV:"
ls -lah
echo "CREATE REPLAY ENVIRONMENT"
"$SCRIPTS_PATH/ci-helpers/prepare_data_and_shm_dir.sh" --data-base-dir="$SOURCE_DATA_DIR" --block-log-source-dir="$BLOCK_LOG_SOURCE_DIR_5M" --config-ini-source="$CONFIG_INI_SOURCE"
ls -lah
echo "START FIRST REPLAY: "
# --docker-option="--network=host"
"$SCRIPTS_PATH"/run_hived_img.sh "$HIVED_IMAGE_NAME" --data-dir="$SOURCE_DATA_DIR"/datadir --name=haf-instance-5M --docker-option="--network=bridge" --replay --stop-replay-at-block=1000000 --detach
docker ps

echo "Docker network ls"
docker network ls

wait_for_instance haf-instance-5M

