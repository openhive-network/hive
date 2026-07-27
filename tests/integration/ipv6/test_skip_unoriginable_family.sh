#!/usr/bin/env bash
# Regression test for https://gitlab.syncad.com/hive/hive/-/work_items/861
#
# Verifies that a node which cannot originate IPv6 connections stops redialing
# IPv6 peer candidates on the retry backoff, while IPv4 dialing is unaffected.
#
# Runs two testnet hived containers on a dedicated docker bridge network
# (IPv4-only - the default for user-defined bridges - so the in-container
# address-family probe genuinely reports IPv6 as unoriginable).  The sync node
# is seeded with the witness's IPv4 endpoint plus an unreachable IPv6 seed
# ([2001:db8::5]:2001, documentation prefix).  The first dial of the IPv6 seed
# always happens (seeds are explicit "add once" requests); what must NOT happen
# after the fix is any backoff retry of it from the connect loop.
#
# Usage:
#   ./test_skip_unoriginable_family.sh <testnet-image>
#
# Exit code 0 = all checks passed; nonzero otherwise.
# Note: run against a pre-fix image to confirm the red case - the retry-count
# check must fail there (>= 2 dials of the IPv6 seed within the window).

set -uo pipefail

IMAGE="${1:?Usage: $0 <testnet-image>}"

if docker info &>/dev/null; then
  DOCKER="docker"
else
  DOCKER="sudo docker"
fi

NETWORK="hive-skipfam-net"
WITNESS_NAME="hived-skipfam-witness"
SYNC_NAME="hived-skipfam-sync"
P2P_PORT=2001
HTTP_PORT=8091
IPV6_SEED="[2001:db8::5]:${P2P_PORT}"
OBSERVE_SECONDS=240

WORKDIR="$(mktemp -d /tmp/hive-skipfam-test.XXXXXX)"

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
  $DOCKER network rm "$NETWORK" &>/dev/null
  rm -rf "$WORKDIR"
}
trap cleanup EXIT

write_config() {
  local datadir="$1" witness="$2"
  mkdir -p "$datadir"
  {
    # p2p logger at "all" so the connect-loop dlogs (dial attempts and skips)
    # land in logs/p2p/p2p.log
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
  local name="$1" datadir="$2"
  shift 2
  # Override the image entrypoint: docker_entrypoint.sh rewrites endpoints and
  # would fight the explicit configuration this test depends on.
  $DOCKER run -d --name "$name" --network="$NETWORK" \
    -v "${datadir}:/home/hived/datadir" \
    --entrypoint /home/hived/bin/hived \
    "$IMAGE" \
    --data-dir=/home/hived/datadir \
    --webserver-http-endpoint="0.0.0.0:${HTTP_PORT}" \
    --p2p-endpoint="0.0.0.0:${P2P_PORT}" \
    "$@" > /dev/null
}

container_ip() {
  $DOCKER inspect -f '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' "$1"
}

api_call() {
  local host="$1" method="$2"
  curl -sf --max-time 10 "http://${host}:${HTTP_PORT}" \
    -d "{\"jsonrpc\":\"2.0\",\"method\":\"${method}\",\"params\":{},\"id\":1}" 2>/dev/null
}

wait_for_api() {
  local host="$1" max_wait="${2:-90}" elapsed=0
  while [ $elapsed -lt "$max_wait" ]; do
    if api_call "$host" "database_api.get_dynamic_global_properties" | grep -q head_block_number; then
      return 0
    fi
    sleep 3
    elapsed=$((elapsed + 3))
  done
  return 1
}

peer_count() {
  api_call "$1" "network_node_api.get_connected_peers" | python3 -c '
import sys, json
r = json.load(sys.stdin).get("result", {})
peers = r.get("connected_peers", []) if isinstance(r, dict) else r
print(len(peers))' 2>/dev/null || echo 0
}

count_ipv6_seed_dials() {
  local log="${WORKDIR}/sync/logs/p2p/p2p.log"
  [ -f "$log" ] || { echo 0; return; }
  grep -c "connect_to_endpoint.*2001:db8::5" "$log" || true
}

# --- Start an IPv4-only two-node network -------------------------------------

info "Using image: $IMAGE  workdir: $WORKDIR"
$DOCKER rm -f "$WITNESS_NAME" "$SYNC_NAME" &>/dev/null
$DOCKER network rm "$NETWORK" &>/dev/null
$DOCKER network create "$NETWORK" > /dev/null

write_config "${WORKDIR}/witness" yes
write_config "${WORKDIR}/sync" no

info "Starting witness node"
start_node "$WITNESS_NAME" "${WORKDIR}/witness"
WITNESS_IP=$(container_ip "$WITNESS_NAME")
wait_for_api "$WITNESS_IP" || { fail "witness node API not ready"; exit 1; }
info "Witness at ${WITNESS_IP}:${P2P_PORT}"

info "Starting sync node with seeds ${WITNESS_IP}:${P2P_PORT} and ${IPV6_SEED}"
start_node "$SYNC_NAME" "${WORKDIR}/sync" \
  --p2p-seed-node="${WITNESS_IP}:${P2P_PORT}" \
  --p2p-seed-node="${IPV6_SEED}"
SYNC_IP=$(container_ip "$SYNC_NAME")
wait_for_api "$SYNC_IP" || { fail "sync node API not ready"; exit 1; }

# --- Check 1: the IPv4 connection works --------------------------------------

CONNECTED=""
for _ in $(seq 1 30); do
  if [ "$(peer_count "$WITNESS_IP")" -ge 1 ]; then
    CONNECTED=yes
    break
  fi
  sleep 2
done
if [ -n "$CONNECTED" ]; then
  pass "IPv4 connection established (family gating does not overreach)"
else
  fail "nodes did not connect over IPv4 within timeout"
fi

# --- Observe the IPv6 seed dialing behavior ----------------------------------
# Retry backoff is (failures+1) * 30s, so a pre-fix node redials the IPv6 seed
# at roughly t+60s and t+150s after the initial failed dial.

info "Observing IPv6 seed dial attempts for ${OBSERVE_SECONDS}s..."
sleep "$OBSERVE_SECONDS"

DIALS=$(count_ipv6_seed_dials)
info "Dial attempts of ${IPV6_SEED}: ${DIALS}"

# --- Check 2: exactly the one explicit seed dial, no backoff retries ---------

if [ "$DIALS" -eq 1 ]; then
  pass "IPv6 seed dialed exactly once (the explicit add-once attempt); no backoff retries"
elif [ "$DIALS" -eq 0 ]; then
  fail "IPv6 seed was never dialed - add-once seed handling changed unexpectedly"
else
  fail "IPv6 seed dialed ${DIALS} times - connect loop is retrying an unoriginable family"
fi

# --- Check 3: the skip is deliberate and logged ------------------------------

if grep -q "cannot originate connections to its address family" "${WORKDIR}/sync/logs/p2p/p2p.log" 2>/dev/null; then
  pass "connect loop logged the family-based skip"
else
  fail "no family-skip log line found in sync node's p2p log"
fi

if grep -q "address families this node can originate" "${WORKDIR}/sync/logs/p2p/p2p.log" 2>/dev/null; then
  pass "probe logged the family availability change (IPv6 unavailable in container)"
else
  fail "no family-availability log line found in sync node's p2p log"
fi

# --- Check 4: IPv4 peer still connected at the end ---------------------------

if [ "$(peer_count "$SYNC_IP")" -ge 1 ]; then
  pass "IPv4 connection still up after observation window"
else
  fail "IPv4 connection lost during observation window"
fi

echo
if [ "$FAILURES" -eq 0 ]; then
  pass "All checks passed"
  exit 0
else
  fail "$FAILURES check(s) failed"
  exit 1
fi
