#! /bin/bash

set -euo pipefail


stop_instance() {
  local container_name="$1"
  docker stop "$container_name"
  docker wait "$container_name" || true
}

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
