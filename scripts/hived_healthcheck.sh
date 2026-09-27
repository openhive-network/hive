#! /bin/bash
# Health check for a dockerized hived: healthy when the node answers JSON-RPC and
# its head block is recent. Meant for a compose/Docker healthcheck: exits 0 when
# healthy, 1 otherwise, and prints a one-line reason (visible in `docker inspect`).
#
#   HIVED_HEALTHCHECK_URL            JSON-RPC endpoint, default http://127.0.0.1:${HTTP_PORT:-8091}/
#   HIVED_HEALTHCHECK_MAX_BLOCK_AGE  seconds the head block may lag behind the wall clock, default 120

set -uo pipefail

URL="${HIVED_HEALTHCHECK_URL:-http://127.0.0.1:${HTTP_PORT:-8091}/}"
MAX_AGE="${HIVED_HEALTHCHECK_MAX_BLOCK_AGE:-120}"
REQUEST='{"jsonrpc":"2.0","method":"database_api.get_dynamic_global_properties","id":1}'

if ! response=$(wget --quiet --timeout=5 --tries=1 --output-document=- \
                     --header='Content-Type: application/json' --post-data="$REQUEST" "$URL"); then
  echo "hived is not answering on ${URL} (still starting or replaying?)"
  exit 1
fi

head_num=$(sed -n 's/.*"head_block_number":\([0-9]*\).*/\1/p' <<<"$response")
head_time=$(sed -n 's/.*"time":"\([0-9T:-]*\)".*/\1/p' <<<"$response")
if [[ -z "$head_num" || -z "$head_time" ]]; then
  echo "unexpected response from ${URL}: ${response:0:200}"
  exit 1
fi

# Block timestamps are UTC without a zone suffix.
age=$(( $(date -u +%s) - $(date -u -d "${head_time}Z" +%s) ))
if (( age > MAX_AGE )); then
  echo "head block ${head_num} is ${age}s old (limit ${MAX_AGE}s): still syncing"
  exit 1
fi

echo "head block ${head_num} is ${age}s old"
