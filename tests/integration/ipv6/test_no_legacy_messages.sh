#!/usr/bin/env bash
# Regression test for https://gitlab.syncad.com/hive/hive/-/work_items/860
#
# Verifies that a P2P connection established over IPv6:
#   1. completes the handshake using the new (IPv6-aware) hello message,
#   2. exchanges no legacy (IPv4-only) messages,
#   3. tears down without the "Unexpected exception from peer_connection's
#      accept_or_connect_task" symptom caused by fc::ip::legacy_address /
#      fc::ip::legacy_endpoint throwing on native IPv6 addresses.
#
# Runs two testnet hived containers on this host connected over [::1].
# Requires docker and a testnet image (e.g. registry.gitlab.syncad.com/hive/hive/testnet:<tag>).
#
# Usage:
#   ./test_no_legacy_messages.sh <testnet-image>
#
# Exit code 0 = all checks passed; nonzero otherwise.
# Note: run against a pre-fix image to confirm the red case — check 3 must fail there.

set -uo pipefail

IMAGE="${1:?Usage: $0 <testnet-image>}"

if docker info &>/dev/null; then
  DOCKER="docker"
else
  DOCKER="sudo docker"
fi

WITNESS_NAME="hived-nolegacy-witness"
SYNC_NAME="hived-nolegacy-sync"
WITNESS_P2P=12101
WITNESS_HTTP=18191
SYNC_P2P=12102
SYNC_HTTP=18192

WORKDIR="$(mktemp -d /tmp/hive-nolegacy-test.XXXXXX)"

RED='\033[0;31m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
NC='\033[0m'
pass() { echo -e "${GREEN}[PASS]${NC} $*"; }
fail() { echo -e "${RED}[FAIL]${NC} $*"; FAILURES=$((FAILURES + 1)); }
info() { echo -e "${CYAN}[INFO]${NC} $*"; }
FAILURES=0

cleanup() {
  $DOCKER rm -f "$WITNESS_NAME" "$SYNC_NAME" &>/dev/null
  rm -rf "$WORKDIR"
}
trap cleanup EXIT

write_config() {
  local datadir="$1" witness="$2"
  mkdir -p "$datadir"
  {
    # p2p logger at "all" so node_impl::on_message's per-message dlog (which names
    # the handled message type) lands in logs/p2p/p2p.log
    echo 'log-appender = {"appender":"stderr","stream":"std_error","time_format":"iso_8601_milliseconds"} {"appender":"p2p","file":"logs/p2p/p2p.log","time_format":"iso_8601_milliseconds"}'
    echo 'log-logger = {"name":"default","level":"info","appender":"stderr"} {"name":"p2p","level":"all","appender":"p2p"}'
    echo 'shared-file-size = 1G'
    echo 'required-participation = 0'
    echo 'plugin = chain p2p webserver json_rpc'
    echo 'plugin = database_api network_node_api'
    if [ "$witness" = "yes" ]; then
      echo 'enable-stale-production = true'
      echo 'plugin = witness'
      echo 'witness = "initminer"'
      echo 'private-key = 5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n'
    fi
  } > "${datadir}/config.ini"
}

start_node() {
  local name="$1" datadir="$2" p2p_port="$3" http_port="$4" seed="$5"
  local seed_arg=()
  [ -n "$seed" ] && seed_arg=(--p2p-seed-node="$seed")
  # Override the image entrypoint: docker_entrypoint.sh rewrites endpoints to 0.0.0.0,
  # which would defeat the IPv6 binding this test depends on.
  $DOCKER run -d --name "$name" --network=host \
    -v "${datadir}:/home/hived/datadir" \
    --entrypoint /home/hived/bin/hived \
    "$IMAGE" \
    --data-dir=/home/hived/datadir \
    --webserver-http-endpoint="127.0.0.1:${http_port}" \
    --p2p-endpoint="[::1]:${p2p_port}" \
    "${seed_arg[@]}" > /dev/null
}

api_call() {
  local port="$1" method="$2"
  curl -sf --max-time 10 "http://127.0.0.1:${port}" \
    -d "{\"jsonrpc\":\"2.0\",\"method\":\"${method}\",\"params\":{},\"id\":1}" 2>/dev/null
}

wait_for_api() {
  local port="$1" max_wait="${2:-90}" elapsed=0
  while [ $elapsed -lt "$max_wait" ]; do
    if api_call "$port" "database_api.get_dynamic_global_properties" | grep -q head_block_number; then
      return 0
    fi
    sleep 3
    elapsed=$((elapsed + 3))
  done
  return 1
}

get_peer_addresses() {
  api_call "$1" "network_node_api.get_connected_peers" | python3 -c '
import sys, json
r = json.load(sys.stdin).get("result", {})
peers = r.get("connected_peers", []) if isinstance(r, dict) else r
for p in peers:
    print(p.get("host", ""))' 2>/dev/null
}

# --- Start both nodes over IPv6 loopback -------------------------------------

info "Using image: $IMAGE  workdir: $WORKDIR"
$DOCKER rm -f "$WITNESS_NAME" "$SYNC_NAME" &>/dev/null

write_config "${WORKDIR}/witness" yes
write_config "${WORKDIR}/sync" no

info "Starting witness node on [::1]:${WITNESS_P2P}"
start_node "$WITNESS_NAME" "${WORKDIR}/witness" "$WITNESS_P2P" "$WITNESS_HTTP" ""
wait_for_api "$WITNESS_HTTP" || { fail "witness node API not ready"; exit 1; }

info "Starting sync node on [::1]:${SYNC_P2P}, seed [::1]:${WITNESS_P2P}"
start_node "$SYNC_NAME" "${WORKDIR}/sync" "$SYNC_P2P" "$SYNC_HTTP" "[::1]:${WITNESS_P2P}"
wait_for_api "$SYNC_HTTP" || { fail "sync node API not ready"; exit 1; }

# --- Check 1: nodes connect to each other over IPv6 --------------------------

info "Waiting for the P2P connection over ::1"
CONNECTED=""
for _ in $(seq 1 30); do
  if get_peer_addresses "$WITNESS_HTTP" | grep -q "::1"; then
    CONNECTED=yes
    break
  fi
  sleep 2
done
if [ -n "$CONNECTED" ]; then
  pass "witness sees a peer connected over ::1"
else
  fail "nodes did not connect over ::1 within timeout"
fi

# let the handshake and a little traffic settle into the logs
sleep 6

# --- Check 2: new hello handled, no legacy messages exchanged ----------------
# Message types (see core_message_type_enum): new IPv6-aware types are 5018-5022,
# legacy IPv4-only types are 5006, 5008, 5010, 5014, 5015.  Match both the enum
# name and the raw value in case the log formatter prints numbers.

for side in witness sync; do
  P2P_LOG="${WORKDIR}/${side}/logs/p2p/p2p.log"
  if [ ! -s "$P2P_LOG" ]; then
    fail "$side: p2p log missing or empty at $P2P_LOG"
    continue
  fi
  if grep -E "handling message (hello_message_type|5018) " "$P2P_LOG" > /dev/null; then
    pass "$side: handled new IPv6-aware hello"
  else
    fail "$side: no new hello_message handled (handshake used unexpected path)"
  fi
  if grep -E "handling message (legacy_[a-z_]+|5006|5008|5010|5014|5015) " "$P2P_LOG" > /dev/null; then
    fail "$side: legacy message received on an IPv6 connection:"
    grep -E "handling message (legacy_[a-z_]+|5006|5008|5010|5014|5015) " "$P2P_LOG" | head -5
  else
    pass "$side: no legacy messages received"
  fi
done

# --- Check 3: clean teardown, no legacy-conversion exception -----------------

info "Stopping both nodes to exercise connection teardown"
$DOCKER stop --time 60 "$SYNC_NAME" "$WITNESS_NAME" > /dev/null

for side in witness sync; do
  container=$([ "$side" = witness ] && echo "$WITNESS_NAME" || echo "$SYNC_NAME")
  P2P_LOG="${WORKDIR}/${side}/logs/p2p/p2p.log"
  SYMPTOM="Unexpected exception from peer_connection's accept_or_connect_task"
  if { [ -f "$P2P_LOG" ] && grep -F "$SYMPTOM" "$P2P_LOG"; } || \
     $DOCKER logs "$container" 2>&1 | grep -F "$SYMPTOM"; then
    fail "$side: legacy-conversion exception surfaced at connection teardown (issue #860 symptom)"
  else
    pass "$side: teardown clean (no accept_or_connect_task exception)"
  fi
done

echo
if [ "$FAILURES" -eq 0 ]; then
  pass "All checks passed"
  exit 0
else
  fail "$FAILURES check(s) failed"
  exit 1
fi
