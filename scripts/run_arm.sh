export CROSS_ROOT=/storage1/src/buildroot/output/host
export CROSS_TRIPPLE=aarch64-buildroot-linux-gnu
export ARCH=aarch64
export CROSS_COMPILE=${CROSS_TRIPPLE}

export QEMU_LD_PREFIX="${CROSS_ROOT}/${CROSS_TRIPPLE}/sysroot"
export QEMU_SET_ENV="LD_LIBRARY_PATH=${CROSS_ROOT}/lib:${QEMU_LD_PREFIX}"

#"${@}"
