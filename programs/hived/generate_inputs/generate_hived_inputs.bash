#!/bin/bash

set -xeuo pipefail

SCRIPTPATH="$(cd -- "$(dirname "$0")" >/dev/null 2>&1; pwd -P)"
BASE_DIR="${SCRIPTPATH}"
PROJECT_ROOT_DIR="$(realpath "${BASE_DIR}/../../..")"
PATH_TO_API_GENERATION="${PROJECT_ROOT_DIR}/libraries/plugins/apis/api_generation/api_generation"

poetry run -C "${PATH_TO_API_GENERATION}" python \
    "${BASE_DIR}/generate_hived_inputs.py" \
    "hived" \
    "${BASE_DIR}/.." \
    "${BASE_DIR}/hived_inputs" \
    "${PATH_TO_API_GENERATION}/pyproject.toml" 

