#!/bin/bash
set -e

HIVE_LINT=${5:OFF}

LOW_MEMORY_NODE=ON
CLEAR_VOTES=ON
BUILD_HIVE_TESTNET=OFF
ENABLE_MIRA=OFF

echo "PWD=${PWD}"
echo "LOW_MEMORY_NODE=${LOW_MEMORY_NODE}"
echo "CLEAR_VOTES=${CLEAR_VOTES}"
echo "BUILD_HIVE_TESTNET=${BUILD_HIVE_TESTNET}"
echo "ENABLE_MIRA=${ENABLE_MIRA}"
echo "HIVE_LINT=${HIVE_LINT}"

BUILD_DIR="${PWD}/build"
CMAKE_BUILD_TYPE=Release
TIME_BEGIN=$( date -u +%s )

git submodule update --init --recursive
mkdir -p "${BUILD_DIR}"
cd ${BUILD_DIR}
cmake \
    -DCMAKE_INSTALL_PREFIX="${BUILD_DIR}/install-root" \
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} \
    -DLOW_MEMORY_NODE=${LOW_MEMORY_NODE} \
    -DCLEAR_VOTES=${CLEAR_VOTES} \
    -DSKIP_BY_TX_ID=OFF \
    -DBUILD_HIVE_TESTNET=${BUILD_HIVE_TESTNET} \
    -DENABLE_MIRA=${ENABLE_MIRA} \
    -DHIVE_STATIC_BUILD=ON \
    -DHIVE_LINT=${HIVE_LINT} \
    .. 
make -j$(nproc)
make install
cd .. 

( "${BUILD_DIR}/install-root"/bin/hived --version \
  | grep -o '[0-9]*\.[0-9]*\.[0-9]*' \
  && echo '_' \
  && git rev-parse --short HEAD ) \
  | sed -e ':a' -e 'N' -e '$!ba' -e 's/\n//g' \
  > "${BUILD_DIR}/install-root"/hived_version && \
cat "${BUILD_DIR}/install-root"/hived_version ;
