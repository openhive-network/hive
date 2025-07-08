#! /bin/bash

set -xeuo pipefail

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"

BASE_DIR="${SCRIPTPATH}"

API_SPEC=${1:-"*_api"}

poetry -vv install

for d in ${API_SPEC}; do
  echo "Processing API: $d"

  poetry run python "${BASE_DIR}"/api_generation/generate_api_definitions.py "$d" "${BASE_DIR}"

  pushd "$d"
  poetry build
  popd

done
