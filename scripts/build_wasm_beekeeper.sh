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

build() {
  CONFIG="$1"
  BUILD_DIR="${EXECUTION_PATH}/programs/beekeeper/beekeeper_wasm/src/build/${CONFIG}"
  mkdir -vp "${BUILD_DIR}"
  cd "${BUILD_DIR}"

  #-DBoost_DEBUG=TRUE -DBoost_VERBOSE=TRUE -DCMAKE_STATIC_LIBRARY_SUFFIX=".a;.bc"
  cmake \
    -DBoost_NO_WARN_NEW_VERSIONS=1 \
    -DBoost_USE_STATIC_RUNTIME=ON \
    -DCMAKE_TOOLCHAIN_FILE=/emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake -DTARGET_ENVIRONMENT="${CONFIG}" -DCMAKE_BUILD_TYPE=Release -G "Ninja" \
    -S "${EXECUTION_PATH}/programs/beekeeper/beekeeper_wasm/" \
    -B "${BUILD_DIR}"
  ninja -v -j8 2>&1 | tee -i "${BUILD_DIR}/build.log"

  cmake --install "${BUILD_DIR}" --component beekeeper.common_runtime --prefix "${BUILD_DIR}/../"
  cmake --install "${BUILD_DIR}" --component beekeeper.common_dts --prefix "${BUILD_DIR}/../"
}

if [ ${DIRECT_EXECUTION} -eq 0 ]; then
  echo "Performing a docker run"
  docker run \
    -it --rm \
    -v "${PROJECT_DIR}/":"${EXECUTION_PATH}" \
    -u $(id -u):$(id -g) \
  registry.gitlab.syncad.com/hive/common-ci-configuration/emsdk:4.0.1-1@sha256:2212741aa9c6647ece43a9ac222c677589f18b27cc8e39921636f125705155dd \
  /bin/bash /src/scripts/build_wasm_beekeeper.sh 1 "${EXECUTION_PATH}"
else
  echo "Performing a build..."
  cd "${EXECUTION_PATH}"
  build "web"
  build "node"
fi
