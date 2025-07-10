#! /bin/bash

set -xeuo pipefail

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
BASE_DIR="${SCRIPTPATH}"

API_GENERATION_PACKAGE_DIR="${BASE_DIR}/api_generation"

API_LIST=("database_api" "block_api" "rc_api" "network_broadcast_api" "account_by_key_api")

for d in "${API_LIST[@]}"; do
  echo "Processing API: $d"

  poetry run -C "${API_GENERATION_PACKAGE_DIR}" python  "${BASE_DIR}"/api_generation/generate_api_definitions.py "$d" "${BASE_DIR}"

done