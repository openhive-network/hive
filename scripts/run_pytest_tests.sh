#!/bin/bash

set -euo pipefail 

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
SCRIPTSDIR="${SCRIPTPATH}"
SRCDIR="${SCRIPTSDIR}/.."

export TEST_SUBDIR=${1:?"Missing arg #1 to specify tests subdirectory"}
shift

export HIVE_BIN_ROOT_PATH=${1:?"Missing arg #2 to specify binaries root directory"}
shift

export HIVED_PATH="${HIVE_BIN_ROOT_PATH}/programs/hived/hived"
export CLI_WALLET_PATH="${HIVE_BIN_ROOT_PATH}/programs/cli_wallet/cli_wallet"
export GET_DEV_KEY_PATH="${HIVE_BIN_ROOT_PATH}/programs/util/get_dev_key"
export SIGN_TRANSACTION_PATH="${HIVE_BIN_ROOT_PATH}/programs/util/sign_transaction"

export TEST_TOOLS_NODE_DEFAULT_WAIT_FOR_LIVE_TIMEOUT="60"

export PYTEST_TIMEOUT_MINUTES=40
export PYTEST_NUMBER_OF_PROCESSES=8
export PYTEST_LOG_DURATIONS=1

export DURATIONS="--durations=0"
export PROCESSES=" --numprocesses=${PYTEST_NUMBER_OF_PROCESSES} "

python3 -m venv "${HIVE_BIN_ROOT_PATH}venv/"
source "${HIVE_BIN_ROOT_PATH}venv/bin/activate"

(pushd "${SRCDIR}/tests/hive-local-tools" && poetry install && popd)

echo "Launching pytest
  - timeout (minutes): ${PYTEST_TIMEOUT_MINUTES}
  - processes: ${PYTEST_NUMBER_OF_PROCESSES}
  - log durations: ${PYTEST_LOG_DURATIONS}
  - additional arguments: $@"

pytest -v --timeout=$(($PYTEST_TIMEOUT_MINUTES * 60)) --junitxml="./report.xml" ${PROCESSES} ${DURATIONS} "${SRCDIR}/${TEST_SUBDIR}" "$@"

deactivate
