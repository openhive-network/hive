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
  BUILD_DIR="${EXECUTION_PATH}/programs/beekeeper/beekeeper_wasm/src/build"
  mkdir -vp "${BUILD_DIR}"
  cd "${BUILD_DIR}"

  #-DBoost_DEBUG=TRUE -DBoost_VERBOSE=TRUE -DCMAKE_STATIC_LIBRARY_SUFFIX=".a;.bc"
  cmake \
    -DBoost_NO_WARN_NEW_VERSIONS=1 \
    -DBoost_USE_STATIC_RUNTIME=ON \
    -DCMAKE_TOOLCHAIN_FILE=/emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake -DCMAKE_BUILD_TYPE=Release -G "Ninja" \
    -S "${EXECUTION_PATH}/programs/beekeeper/beekeeper_wasm/" \
    -B "${BUILD_DIR}"
  ninja -v -j8 2>&1 | tee -i "${BUILD_DIR}/build.log"

  cmake --install "${BUILD_DIR}" --component wasm_runtime_components --prefix "${BUILD_DIR}/"

  # Emscripten still uses redundant createRequire for legacy CJS support - remove it so we have proper bundlers support
  sed -i "s#var require = createRequire(import.meta.url);##g" "${BUILD_DIR}/beekeeper_wasm.node.js"
  sed -i "s#const {createRequire} = await import(\"module\");##g" "${BUILD_DIR}/beekeeper_wasm.node.js"

  # Replace requires with our await import-s
  sed -i "s#require(\"fs\");#(await import(\"fs\"))#g" "${BUILD_DIR}/beekeeper_wasm.node.js"
  sed -i "s#require(\"path\")#(await import(\"path\"))#g" "${BUILD_DIR}/beekeeper_wasm.node.js"
  sed -i "s#require(\"url\")#(await import(\"url\"))#g" "${BUILD_DIR}/beekeeper_wasm.node.js"

  # Remove Node.js "crypto" module import, as we already have crypto API support in Node.js 19+
  sed -i "s#var nodeCrypto = require(\"crypto\");##g" "${BUILD_DIR}/beekeeper_wasm.node.js"
  sed -i "s#return view => nodeCrypto.randomFillSync(view);##g" "${BUILD_DIR}/beekeeper_wasm.node.js"
}

if [ ${DIRECT_EXECUTION} -eq 0 ]; then
  echo "Performing a docker run"
  docker run \
    -it --rm \
    -v "${PROJECT_DIR}/":"${EXECUTION_PATH}" \
    -u $(id -u):$(id -g) \
  registry.gitlab.syncad.com/hive/common-ci-configuration/emsdk:4.0.18-1@sha256:79edc8ecfe7b13848466d33791daa955eb1762edc329a48c07aa700bc6cfb712 \
  /bin/bash /src/scripts/build_wasm_beekeeper.sh 1 "${EXECUTION_PATH}"
else
  echo "Performing a build..."
  cd "${EXECUTION_PATH}"
  build
fi
