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

_conftest_started_at="${_conftest_started_at:-$(date +%s)}"

# A step fails once it makes no progress for the stall window, or at the ceiling:
# the time the run has left, less a margin for the teardown after it.
_STEP_CEILING_MARGIN=60

_positive_int() {  # NAME VALUE
  [[ "$2" =~ ^[1-9][0-9]*$ ]] && return 0
  echo "ERROR: $1 must be a positive integer, got '$2'" >&2
  return 1
}

# The epoch second the run has to end by: what AIDEV's integration step gives the
# script (HIVE_INTEGRATION_DEADLINE), else CI's job end (CI_JOB_STARTED_AT plus
# CI_JOB_TIMEOUT), and never later than the 30 minutes CI's `timeout 30m` gives
# the script from its start.
_run_deadline() {
  local by=$(( _conftest_started_at + 1800 )) other="" started
  if [ -n "${HIVE_INTEGRATION_DEADLINE:-}" ]; then
    _positive_int HIVE_INTEGRATION_DEADLINE "$HIVE_INTEGRATION_DEADLINE" || return 1
    other=$HIVE_INTEGRATION_DEADLINE
  elif [ -n "${CI_JOB_STARTED_AT:-}" ] && [[ "${CI_JOB_TIMEOUT:-}" =~ ^[1-9][0-9]*$ ]] \
      && started=$(date -d "$CI_JOB_STARTED_AT" +%s 2>/dev/null); then
    other=$(( started + CI_JOB_TIMEOUT ))
  fi
  [ -n "$other" ] && [ "$other" -lt "$by" ] && by=$other
  echo "$by"
}

# Polls PROBE until it returns 0 (done); it returns 1 to keep waiting and 2 or more
# to fail. PROBE runs in the caller's scope, so it can keep state in the caller's
# locals, and sets `_mark` to the progress it sees: a change of the mark is progress,
# an empty one is no reading. Fails as `stalled` after HIVE_INTEGRATION_STALL_SECONDS
# (300) without progress (returning 1), and as `ceiling` at the run's deadline
# (_run_deadline) or LIMIT seconds from now, whichever is earlier (returning 124, as
# timeout(1) does: .aidev/integration.sh takes it for the deadline's).
_await_progress() {  # WHAT LIMIT|"" PROBE...
  local - what="$1" limit="$2" stall="${HIVE_INTEGRATION_STALL_SECONDS:-300}" poll="${HIVE_INTEGRATION_POLL_SECONDS:-10}"
  local started now last_progress ceiling mark="" _mark rc
  set +x
  shift 2
  _positive_int HIVE_INTEGRATION_STALL_SECONDS "$stall" && _positive_int HIVE_INTEGRATION_POLL_SECONDS "$poll" || return 2
  ceiling=$(_run_deadline) || return 2
  started=$(date +%s)
  last_progress=$started
  ceiling=$(( ceiling - _STEP_CEILING_MARGIN ))
  if [ -n "$limit" ]; then
    _positive_int "$what's limit" "$limit" || return 2
    [ $(( started + limit )) -lt "$ceiling" ] && ceiling=$(( started + limit ))
  fi
  echo "$what: fails after ${stall}s without progress, or at the ceiling in $(( ceiling - started ))s" >&2

  while :; do
    _mark=""
    rc=0
    "$@" || rc=$?
    [ "$rc" -eq 1 ] || return "$rc"
    now=$(date +%s)
    if [ -n "$_mark" ] && [ "$_mark" != "$mark" ]; then
      mark=$_mark
      last_progress=$now
    fi
    if [ $(( now - last_progress )) -ge "$stall" ]; then
      echo "ERROR: $what stalled: no progress for $(( now - last_progress ))s (last progress: ${mark:-none}; $(( now - started ))s elapsed)" >&2
      return 1
    fi
    if [ "$now" -ge "$ceiling" ]; then
      echo "ERROR: $what reached the ceiling after $(( now - started ))s, still progressing (last progress: ${mark:-none})" >&2
      return 124
    fi
    sleep $(( ceiling - now < poll ? ceiling - now : poll ))
  done
}

# Runs COMMAND, then logs `step NAME: <seconds>s` to stderr, whether it succeeded or not.
run_step() {  # NAME COMMAND...
  local name="$1" started rc=0
  shift
  started=$(date +%s)
  "$@" || rc=$?
  echo "step $name: $(( $(date +%s) - started ))s" >&2
  return "$rc"
}

_log_bytes() {  # FILE FROM TO: bytes FROM+1..TO of FILE
  tail -c "+$(( $2 + 1 ))" "$1" | head -c "$(( $3 - $2 ))"
}

# The highest block number the sync (`Got block: #N`) and replay (`N of M blocks`)
# lines on stdin carry; empty when there is none.
_highest_logged_block() {
  grep -oE 'Got block: #[0-9]+|[0-9]+ of [0-9]+ blocks' \
    | grep -oE '^[^0-9]*[0-9]+' | grep -oE '[0-9]+$' | sort -n | tail -1
}

# monitor_container's probe: prints the container's new log output; the progress
# is the highest block number its sync and replay lines carry, or before the first
# one, the log's size. Done when the container is no longer running.
_container_progress() {
  local size status b
  status=$(docker inspect --format '{{.State.Status}}' "$container_name") || {
    echo "ERROR: cannot inspect container '$container_name'" >&2
    return 2
  }
  size=$(stat -c %s "$log")
  if [ "$size" -gt "$offset" ]; then
    _log_bytes "$log" "$offset" "$size"
    b=$(_log_bytes "$log" "$offset" "$size" | _highest_logged_block)
    [ -n "$b" ] && { [ -z "$block" ] || [ "$b" -gt "$block" ]; } && block=$b
    offset=$size
  fi
  if [ -n "$block" ]; then _mark="block $block"; else _mark="log $size bytes"; fi
  case "$status" in exited|dead) return 0 ;; esac
  return 1
}

# Follows the container's log until it exits; fails (stopping it) when it stalls or
# reaches the ceiling (_await_progress, whose status it returns), and when it exits
# non-zero. SECONDS, if given, caps the ceiling.
monitor_container() {  # CONTAINER [SECONDS]
    local container_name="$1" limit="${2:-}" log logs_pid offset=0 block="" rc=0

    if [ -z "$container_name" ]; then
        echo "Usage: monitor_container <container_name> [max_seconds]"
        return 1
    fi

    echo "Monitoring container '$container_name'..."

    log=$(mktemp)
    docker logs -f "$container_name" > "$log" 2>&1 &
    logs_pid=$!

    _await_progress "container '$container_name'" "$limit" _container_progress || rc=$?
    [ "$rc" -eq 0 ] || kill "$logs_pid" 2>/dev/null || true
    wait "$logs_pid" 2>/dev/null || true
    _log_bytes "$log" "$offset" "$(stat -c %s "$log")"
    rm -f "$log"

    if [ "$rc" -ne 0 ]; then
        docker stop "$container_name" || true
        return "$rc"
    fi

    local EXIT_CODE
    EXIT_CODE=$(docker inspect "$container_name" --format '{{.State.ExitCode}}')

    echo "Container exit code: $EXIT_CODE"

    if [ "$EXIT_CODE" -ne 0 ]; then
        echo "Error: container exited with code $EXIT_CODE"
        return 1
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

_api_ready_progress() {
  local app_status
  app_status=$(docker exec "$container" wget -O - -q --timeout=10 \
    --post-data='{"jsonrpc":"2.0","method":"app_status_api.get_app_status","id":1}' \
    localhost:8091 2>/dev/null || echo "")

  if [ -n "$app_status" ] && grep -q 'chain API ready' <<< "$app_status"; then
    return 0
  fi
  _mark="log $(docker logs "$container" 2>&1 | wc -c) bytes"
  echo "Waiting for API mode ($_mark)..." >&2
  return 1
}

# Wait for chain API to become ready (checks app_status_api for 'chain API ready' status).
# Progress is new log output (the snapshot load); see _await_progress. SECONDS, if
# given, caps the ceiling. Returns 0 if ready, else _await_progress's status.
wait_for_chain_api_ready() {  # CONTAINER [SECONDS]
  local container=$1 started rc=0
  started=$(date +%s)

  _await_progress "API mode of '$container'" "${2:-}" _api_ready_progress || rc=$?
  [ "$rc" -eq 0 ] || return "$rc"
  echo "Chain API ready after $(( $(date +%s) - started ))s" >&2
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

# wait_for_container_block's probe: the progress is the highest block either
# database_api's head block or, while that does not answer (as when hived applies
# synced blocks), the container's sync and replay log lines have shown. Done only
# when database_api's head block reaches the target.
_head_block_progress() {
  local b now
  head_block=$(get_container_head_block "$container")

  if [ -n "$head_block" ]; then
    echo "$message: head_block_number = $head_block / $target_block" >&2
    [ "$head_block" -ge "$target_block" ] && return 0
    [ -z "$highest" ] || [ "$head_block" -gt "$highest" ] && { highest=$head_block; source="head block"; }
  else
    now=$(date +%s)
    b=$(docker logs ${log_since:+--since "$log_since"} "$container" 2>&1 | _highest_logged_block)
    log_since=$now
    [ -n "$b" ] && { [ -z "$highest" ] || [ "$b" -gt "$highest" ]; } && { highest=$b; source="log block"; }
    echo "$message: waiting for database_api to respond (${source:-no block} ${highest:-yet} / $target_block)..." >&2
  fi
  [ -n "$highest" ] && _mark="$source $highest"
  return 1
}

# Wait for container to reach target block: prints the head block reached, or an
# empty line and returns _await_progress's status when the block it is at (see
# _head_block_progress) stalls or the ceiling comes. SECONDS, if given, caps the
# ceiling.
wait_for_container_block() {  # CONTAINER TARGET_BLOCK MESSAGE [SECONDS]
  local container=$1
  local target_block=$2
  local message=$3
  local head_block="" highest="" source="" log_since="" rc=0

  _await_progress "$message ('$container' to block $target_block)" "${4:-}" _head_block_progress || rc=$?
  if [ "$rc" -ne 0 ]; then
    echo "ERROR: Did not reach target block $target_block (last head_block: ${head_block:-none})" >&2
    echo ""
    return "$rc"
  fi
  echo "$head_block"
}
