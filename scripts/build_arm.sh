#!/usr/bin/env bash
set -x
set -e
set -o pipefail

export CROSS_ROOT=/storage1/src/buildroot/output/host
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

cmake --toolchain ${CROSS_ROOT}/Toolchain.cmake -B build -S. -GNinja && cmake --build build "$@"
