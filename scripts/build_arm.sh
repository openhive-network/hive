#!/usr/bin/env bash
set -x
set -e
set -o pipefail

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"

SRC_DIR="${SCRIPTPATH}/.."
BUILD_DIR="${SRC_DIR}/build_arm/"

mkdir -vp "${BUILD_DIR}"

export CROSS_ROOT=${CROSS_ROOT:-${SRC_DIR}/../buildroot/output/host}
export CROSS_TRIPPLE=aarch64-buildroot-linux-gnu
export ARCH=aarch64
export CROSS_COMPILE=${CROSS_TRIPPLE}

export AS=${CROSS_ROOT}/bin/${CROSS_TRIPPLE}-as
export AR=${CROSS_ROOT}/bin/${CROSS_TRIPPLE}-ar
export CC=${CROSS_ROOT}/bin/${CROSS_TRIPPLE}-gcc
export CPP=${CROSS_ROOT}/bin/${CROSS_TRIPPLE}-cpp
export CXX=${CROSS_ROOT}/bin/${CROSS_TRIPPLE}-g++
export LD=${CROSS_ROOT}/bin/${CROSS_TRIPPLE}-ld

export QEMU_LD_PREFIX="${CROSS_ROOT}/${CROSS_TRIPPLE}/sysroot"
export QEMU_SET_ENV="LD_LIBRARY_PATH=${CROSS_ROOT}/lib:${QEMU_LD_PREFIX}"

cmake --toolchain ${CROSS_ROOT}/Toolchain.cmake -B "${BUILD_DIR}" -S "${SRC_DIR}" -GNinja 2>&1 | tee "${BUILD_DIR}/cmake.log" && cmake --build "${BUILD_DIR}" --target hived 2>&1 | tee "${BUILD_DIR}/build.log"
