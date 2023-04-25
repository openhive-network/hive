#! /bin/bash -x

set -xeuo pipefail

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
"$SCRIPTS_PATH"/run_hived_img.sh "$HIVED_IMAGE_NAME" --data-dir="$SOURCE_DATA_DIR"/datadir --name=haf-instance-5M --replay --stop-replay-at-block=1000000 --docker-option="--network=host" --detach
docker ps
docker inspect haf-instance-5M --format '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' "$@"
CONTAINER_IP=$(docker inspect haf-instance-5M --format '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' "$@")
echo "$CONTAINER_IP"

echo "Docker network ls"
docker network ls

echo "docker network inspect"
docker network inspect
####
echo "Check the number of replayed blocks"
docker exec -i haf-instance-5M bash << EOF
echo "curl_only"
curl curl -s --data '{"jsonrpc": "2.0","method": "database_api.get_dynamic_global_properties","id": 1}' 0.0.0.0:8090
echo "with grep"
curl curl -s --data '{"jsonrpc": "2.0","method": "database_api.get_dynamic_global_properties","id": 1}' 0.0.0.0:8090 | grep -o '"head_block_number":1000000' | grep -o -E "[0-9]+")
EOF
####
echo "Check_2 the number of replayed blocks"
start_time=$(date +%s)
while (( $(date +%s) - start_time < 600 ))
  do
    PORT=8090
    PROBLEM_response="INITIAL VALUE"
    docker ps
    A=$(curl -s --data '{"jsonrpc": "2.0","method": "database_api.get_dynamic_global_properties","id": 1}' "$CONTAINER_IP":"$PORT") || true
    docker ps
    WYNIK_CURL=$(curl -s --data '{"jsonrpc": "2.0","method": "database_api.get_dynamic_global_properties","id": 1}' "$CONTAINER_IP":"$PORT" | grep -o '"head_block_number":1000000' | grep -o -E "[0-9]+") || true
    docker ps
  if [[ $WYNIK_CURL = "1000000" ]]
  then
    docker ps
    echo "docker zaczyna stopowac"
    docker stop "$HIVED_IMAGE_NAME"
    docker ps
    echo "Success! Replayed: $PROBLEM_response blocks."
    docker ps
    break
  else
    sudo lsof -i -P -n | grep LISTEN
    echo "Waiting for replay. Retrying in 2 seconds..."
    sleep 2
  fi
done

#echo "Logs from first container hived_instance:"
#docker logs -f haf-instance-5M &
#test "$(docker wait haf-instance-5M)" = 0
#
#
#docker exec -it "$HIVED_IMAGE_NAME" bash


#echo "START SECOND REPLAY: "
#echo "Logs from second container hived_instance:"
