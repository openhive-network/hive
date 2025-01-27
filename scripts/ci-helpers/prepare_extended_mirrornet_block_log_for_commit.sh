#!/bin/bash

set -e

echo "Preparing environment..."

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 || exit 1; pwd -P )"
SCRIPTSDIR="$SCRIPTPATH/.."
SRC_DIR="$(realpath "${SCRIPTSDIR}/..")"
WORKING_DIR=${CI_PROJECT_DIR:-$(pwd)}

export LOG_FILE=build_instance4commit.log
# shellcheck source=../common.sh
source "$SCRIPTSDIR/common.sh"

HIVE_SRC="${HIVE_SRC:-${SRC_DIR}}"
HIVE_COMMIT="${HIVE_COMMIT:-${CI_COMMIT_SHORT_SHA:-$1}}"

echo "Hive source located at ${HIVE_SRC}. Requested commit: ${HIVE_COMMIT}"

pushd "${HIVE_SRC}"
git fetch
CURRENT_COMMIT=$(git rev-parse --verify HEAD)
TARGET_COMMIT=$(git rev-parse --verify "${HIVE_COMMIT}")

echo -e "Current commit: ${CURRENT_COMMIT}.\nDesired commit: ${TARGET_COMMIT}"

if [ "${TARGET_COMMIT}" != "${CURRENT_COMMIT}" ]; then
    git config --global init.defaultBranch "develop" # Suppress default branch name warning
    do_clone "develop" "${HIVE_SRC}/hive-${TARGET_COMMIT}" https://gitlab.syncad.com/hive/hive.git "${TARGET_COMMIT}"
    HIVE_SRC="${HIVE_SRC}/hive-${TARGET_COMMIT}"
    pushd "${HIVE_SRC}"
    echo "Hive source changed to ${HIVE_SRC}"
fi

"${HIVE_SRC}/scripts/ci-helpers/prepare_extended_mirrornet_block_log.sh"

if ! cmp -s "${HIVE_SRC}/block_log.env" "${WORKING_DIR}/block_log.env"; then
    mv "${HIVE_SRC}/block_log.env" "${WORKING_DIR}/block_log.env"
fi

if [[ -e "${HIVE_SRC}/generated" && $(realpath "${HIVE_SRC}/generated") != $(realpath "${WORKING_DIR}/generated") ]]; then
    mv "${HIVE_SRC}/generated" "${WORKING_DIR}"
fi

if [ "${TARGET_COMMIT}" != "${CURRENT_COMMIT}" ]; then
    popd
    rm -rf "${HIVE_SRC}"
fi

popd