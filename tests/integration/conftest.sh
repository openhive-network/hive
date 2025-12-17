#! /bin/bash

set -euo pipefail


get_head_block_number() {
  local block_num response

  local api_node="${HIVE_API_NODE:-https://api.hive.blog}"

  response=$(curl -s --data '{"jsonrpc":"2.0", "method":"database_api.get_dynamic_global_properties", "id":1}' "$api_node")
  block_num=$(echo "$response" | sed -n 's/.*"head_block_number": *\([0-9]*\).*/\1/p')

  echo "$block_num"
}

get_last_block_log_part_number() {
    local hb last_block_log_part_number

    hb=$(get_head_block_number)
    last_block_log_part_number=$(( (hb / 1000000) * 1000000 ))

    echo "$last_block_log_part_number"
}

monitor_container() {
    local container_name="$1"
    local timeout_seconds="${2:-7200}"  # Default 2 hours

    if [ -z "$container_name" ]; then
        echo "Usage: monitor_container <container_name> [timeout_seconds]"
        exit 1
    fi

    echo "Monitoring container '$container_name' with timeout ${timeout_seconds}s..."

    docker logs -f "$container_name" &
    local logs_pid=$!

    if ! timeout "$timeout_seconds" docker wait "$container_name" >/dev/null; then
        kill $logs_pid 2>/dev/null || true
        echo "ERROR: Container '$container_name' exceeded timeout of ${timeout_seconds}s"
        docker stop "$container_name" || true
        exit 1
    fi

    kill $logs_pid 2>/dev/null || true
    wait $logs_pid 2>/dev/null || true

    local EXIT_CODE
    EXIT_CODE=$(docker inspect "$container_name" --format '{{.State.ExitCode}}')

    echo "Container exit code: $EXIT_CODE"

    if [ "$EXIT_CODE" -ne 0 ]; then
        echo "Error: container exited with code $EXIT_CODE"
        exit 1
    fi
}

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

# Wait for API mode to become ready (checks app_status_api for 'entering API mode' status)
# Returns 0 if ready, 1 if timeout
wait_for_chain_api_ready() {
  local container=$1
  local max_wait_seconds=${2:-900}  # Default 15 minutes
  local elapsed=0
  local interval=30

  echo "Waiting for API mode (timeout: ${max_wait_seconds}s)..." >&2

  while [ $elapsed -lt $max_wait_seconds ]; do
    local app_status=$(docker exec $container wget -O - -q --timeout=10 \
      --post-data='{"jsonrpc":"2.0","method":"app_status_api.get_app_status","id":1}' \
      localhost:8091 2>/dev/null || echo "")

    if [ -n "$app_status" ] && echo "$app_status" | grep -q 'entering API mode'; then
      echo "API mode ready after ${elapsed}s" >&2
      return 0
    fi

    [ $((elapsed % 60)) -eq 0 ] && echo "Waiting for API mode (${elapsed}s elapsed)..." >&2
    sleep $interval
    elapsed=$((elapsed + interval))
  done

  echo "ERROR: API mode not ready within ${max_wait_seconds}s" >&2
  return 1
}

# Get head block number from a Docker container via database_api
# Assumes chain API is already ready (use wait_for_chain_api_ready first)
get_container_head_block() {
  local container=$1

  local head_block=$(docker exec $container wget -O - -q --timeout=30 \
    --post-data='{"jsonrpc":"2.0","method":"database_api.get_dynamic_global_properties","id":1}' \
    localhost:8091 2>/dev/null | grep -o '"head_block_number":[0-9]*' | grep -o '[0-9]*' || echo "")

  if [ -n "$head_block" ]; then
    echo "$head_block"
  else
    echo ""
  fi
}

# Wait for container to reach target block with retry logic
# Retries every 30s until target is reached or timeout (default 2 minutes)
wait_for_container_block() {
  local container=$1
  local target_block=$2
  local message=$3
  local max_wait_seconds=${4:-120}  # Default 2 minutes
  local head_block=""
  local elapsed=0
  local interval=30

  echo "Polling database_api every ${interval}s (timeout: ${max_wait_seconds}s)..." >&2

  while [ $elapsed -lt $max_wait_seconds ]; do
    head_block=$(get_container_head_block "$container")

    if [ -n "$head_block" ]; then
      echo "$message: head_block_number = $head_block / $target_block (${elapsed}s elapsed)"
      if [ "$head_block" -ge "$target_block" ]; then
        echo "$head_block"
        return 0
      fi
    else
      echo "$message: waiting for database_api to respond (${elapsed}s elapsed, retrying in ${interval}s)..."
    fi

    sleep $interval
    elapsed=$((elapsed + interval))
  done

  echo "ERROR: Did not reach target block $target_block within ${max_wait_seconds}s (last head_block: ${head_block:-none})" >&2
  echo ""
  return 1
}
