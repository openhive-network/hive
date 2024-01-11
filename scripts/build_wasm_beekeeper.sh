#! /bin/bash

set -xeuo pipefail

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
PROJECT_DIR="${SCRIPTPATH}/.."

DIRECT_EXECUTION_DEFAULT=0
EXECUTION_PATH_DEFAULT="/src/"

# Check for usage inside dev container providing all tools (emscripten image)
if [ $# -eq 0 ]; then
  EXECUTOR=$(whoami)
  if [ "${EXECUTOR}" = "emscripten" ]; then
    DIRECT_EXECUTION_DEFAULT=1
    EXECUTION_PATH_DEFAULT="${PROJECT_DIR}"
  fi
fi

DIRECT_EXECUTION=${1:-${DIRECT_EXECUTION_DEFAULT}}
EXECUTION_PATH=${2:-"${EXECUTION_PATH_DEFAULT}"}

if [ ${DIRECT_EXECUTION} -eq 0 ]; then
  echo "Performing a docker run"
  docker run \
    -it --rm \
    -v "${PROJECT_DIR}/":"${EXECUTION_PATH}" \
    -u $(id -u):$(id -g) \
  registry.gitlab.syncad.com/hive/common-ci-configuration/emsdk:3.1.51@sha256:cfb5f84348f27ffecf705641894286a45c9226b7d2943ca2937a04946e966662 \
  /bin/bash /src/scripts/build_wasm_beekeeper.sh 1 "${EXECUTION_PATH}"
else
  echo "Performing a build..."
  cd "${EXECUTION_PATH}"
  BUILD_DIR="${EXECUTION_PATH}/programs/beekeeper/beekeeper_wasm/build"
  mkdir -vp "${BUILD_DIR}"
  cd "${BUILD_DIR}"

  #-DBoost_DEBUG=TRUE -DBoost_VERBOSE=TRUE -DCMAKE_STATIC_LIBRARY_SUFFIX=".a;.bc"
  cmake \
    -DBoost_NO_WARN_NEW_VERSIONS=1 \
    -DBoost_USE_STATIC_RUNTIME=ON \
    -DCMAKE_TOOLCHAIN_FILE=/emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake -DCMAKE_BUILD_TYPE=Release -G "Ninja" \
    -S "${EXECUTION_PATH}/programs/beekeeper/beekeeper_wasm/" \
    -B "${BUILD_DIR}"
  ninja -j8
fi
