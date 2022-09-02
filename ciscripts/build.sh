#!/bin/bash
set -e

BUILD_HIVE_TESTNET=$1
HIVE_LINT=${2:OFF}

echo "PWD=${PWD}"
echo "BUILD_HIVE_TESTNET=${BUILD_HIVE_TESTNET}"
echo "HIVE_LINT=${HIVE_LINT}"

BUILD_DIR="${PWD}/build"
CMAKE_BUILD_TYPE=Release
TIME_BEGIN=$( date -u +%s )

git submodule update --init --recursive
mkdir -p "${BUILD_DIR}"
cd ${BUILD_DIR}
echo -e "\e[0Ksection_start:`date +%s`:cmake_section\r\e[0KRunning CMake"
cmake \
    -DCMAKE_INSTALL_PREFIX="${BUILD_DIR}/install-root" \
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} \
    -DBUILD_HIVE_TESTNET=${BUILD_HIVE_TESTNET} \
    -DHIVE_LINT=${HIVE_LINT} \
    -GNinja \
    ..
echo -e "\e[0Ksection_end:`date +%s`:cmake_section\r\e[0K"
echo -e "\e[0Ksection_start:`date +%s`:build_section\r\e[0KBuilding with Ninja"
ninja
echo -e "\e[0Ksection_end:`date +%s`:build_section\r\e[0K"
ldd "${BUILD_DIR}/programs/cli_wallet/cli_wallet"
echo -e "\e[0Ksection_start:`date +%s`:install_section\r\e[0KInstalling with Ninja"
ninja install
echo -e "\e[0Ksection_end:`date +%s`:install_section\r\e[0K"
cd ..

( "${BUILD_DIR}/install-root"/bin/hived --version \
  | grep -o '[0-9]*\.[0-9]*\.[0-9]*' \
  && echo '_' \
  && git rev-parse --short HEAD ) \
  | sed -e ':a' -e 'N' -e '$!ba' -e 's/\n//g' \
  > "${BUILD_DIR}/install-root"/hived_version && \
cat "${BUILD_DIR}/install-root"/hived_version ;

