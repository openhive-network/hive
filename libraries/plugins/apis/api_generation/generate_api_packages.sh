#! /bin/bash

set -xeuo pipefail

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
BASE_DIR="${SCRIPTPATH}"

API_GENERATION_PACKAGE_DIR="${BASE_DIR}/api_generation"

if [ "$#" -gt 0 ]; then
  API_LIST=("$@")
else
  API_LIST=(
    "account_by_key_api"
    "account_history_api"
    "block_api"
    "bridge"
    "condenser_api"
    "database_api"
    "debug_node_api"
    "follow_api"
    "hive"
    "market_history_api"
    "network_broadcast_api"
    "rc_api"
    "reputation_api"
    "rewards_api"
    "search-api"
    "tags_api"
    "transaction_status_api"
    "wallet_bridge_api"
    "witness_api"
)
fi

for d in "${API_LIST[@]}"; do
  echo "Processing API: $d"

  poetry run -C "${API_GENERATION_PACKAGE_DIR}" python  "${BASE_DIR}"/api_generation/generate_api_definitions.py "$d" "${BASE_DIR}"

done
