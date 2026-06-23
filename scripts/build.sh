#! /bin/bash

set -euo pipefail

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"

export LOG_FILE=build.log
# shellcheck source=./common.sh
source "$SCRIPTPATH/common.sh"

log_exec_params "$@"

#This cript builds all (or selected) targets in the hive repo

print_help () {
cat <<EOF
Usage: $0 [OPTION[=VALUE]]... [target]...

Allows to build Hive sources 
  --source-dir=DIRECTORY_PATH
                           Specify a directory containing a Hived source tree.
  --binary-dir=DIRECTORY_PATH
                           Specify a directory to store a build output (hived binaries).
                           Usually it is a \`build\` subdirectory in the hive source tree.
  --flat-binary-directory=DIRECTORY_PATH
                           Directory where all binaries are moved after build while flattening
                           the directory structure, that is installation directory (optional, not set by default, must exist)
  --cmake-arg=ARG          Specify additional arguments to the CMake tool spawn.
  --cross-root=DIRECTORY_PATH
                           Cross-compile for aarch64 using the buildroot toolchain rooted at the
                           given path (e.g. the ci-base-image's \$CROSS_ROOT). This sets up the
                           whole cross environment for you - toolchain compilers, OpenSSL lookup,
                           the qemu-aarch64 emulator, static readline/ncurses and the CMake
                           toolchain file - so no manual environment variables are required.
  --vendor-dir=DIRECTORY_PATH
                           When cross-compiling, use preinstalled aarch64 vendor libs (RocksDB)
                           from this directory. Defaults to \$HIVE_AARCH64_VENDOR_DIR when set
                           (the ci-base-image exports it), otherwise RocksDB is built from source.
  --clean-after-build      Remove compiled files after build
  --haf-build              Set if build is called from HAF
  --jobs=N                 Number of parallel build jobs (default: auto-detected based on CPU/RAM).
  --help                   Display this help screen and exit.

EOF
}

HIVED_BINARY_DIR="../build"
HIVED_SOURCE_DIR="."
HIVED_INSTALLATION_DIR=""
CLEAN_AFTER_BUILD="false"
HAF_BUILD=""
USER_JOBS=""
CROSS_ROOT_ARG=""
VENDOR_DIR_ARG=""

CMAKE_ARGS=()

calculate_jobs() {
  # Calculate parallel jobs: default 20, scale down for systems with less RAM
  local jobs
  jobs=$(nproc)
  jobs=$(( jobs > 20 ? 20 : jobs ))

  # Memory-aware scaling: ~6GB per compile job
  local available_mem_gb
  available_mem_gb=$(awk '/MemAvailable/ {printf "%d", $2/1024/1024}' /proc/meminfo 2>/dev/null || echo "128")
  local mem_based_jobs=$(( available_mem_gb / 6 ))
  mem_based_jobs=$(( mem_based_jobs < 1 ? 1 : mem_based_jobs ))
  if [[ $mem_based_jobs -lt $jobs ]]; then
    jobs=$mem_based_jobs
  fi

  echo "$jobs"
}

add_cmake_arg () {
  CMAKE_ARGS+=("$1")
}

# Configure the whole aarch64 cross-compilation environment from a single --cross-root path.
# This centralises everything the cross build needs (previously duplicated inline in the Dockerfile
# and forced onto anyone building locally): toolchain compilers, OpenSSL lookup, the qemu emulator,
# static readline/ncurses and the CMake toolchain file. sccache is left disabled by the top-level
# CMakeLists when CMAKE_CROSSCOMPILING is set, so nothing to do here for that.
setup_cross_environment () {
  local cross_root="$1"
  local cross_tripple="aarch64-buildroot-linux-gnu"

  if [[ ! -d "$cross_root" ]]; then
    echo "ERROR: --cross-root path does not exist: $cross_root"
    exit 1
  fi
  local toolchain_file="$cross_root/Toolchain.cmake"
  if [[ ! -f "$toolchain_file" ]]; then
    echo "ERROR: CMake toolchain file not found: $toolchain_file"
    echo "       --cross-root must point at a buildroot toolchain that ships Toolchain.cmake"
    echo "       (the ci-base-image provides it at \$CROSS_ROOT/Toolchain.cmake)."
    exit 1
  fi

  echo "Cross-compiling for aarch64 using toolchain at $cross_root"

  # The (x86_64) ci-base-image exports OPENSSL_* pointing at its /usr/local OpenSSL; under the cross
  # toolchain (finds restricted to the sysroot) these misdirect find_package(OpenSSL). Drop them so
  # OpenSSL and the other deps are found in the aarch64 sysroot.
  unset OPENSSL_ROOT_DIR OPENSSL_INCLUDE_DIR OPENSSL_CRYPTO_LIBRARY OPENSSL_SSL_LIBRARY || true

  # Export the cross toolchain so non-CMake sub-builds that read CC/CXX/AR/... from the environment
  # (e.g. fc's autotools secp256k1) also cross-compile instead of using the host x86_64 compiler.
  export ARCH=aarch64
  export CROSS_COMPILE="$cross_tripple"
  export CROSS_ROOT="$cross_root"
  export AS="$cross_root/bin/$cross_tripple-as"   AR="$cross_root/bin/$cross_tripple-ar"
  export CC="$cross_root/bin/$cross_tripple-gcc"  CPP="$cross_root/bin/$cross_tripple-cpp"
  export CXX="$cross_root/bin/$cross_tripple-g++" LD="$cross_root/bin/$cross_tripple-ld"

  # The build RUNs freshly-built aarch64 codegen tools (the protocol/chain assertion-id generators),
  # so a qemu-aarch64 user-mode emulator must be available; Toolchain.cmake auto-detects it and
  # QEMU_LD_PREFIX lets the emulated tools resolve their loader and shared libs from the sysroot.
  local sysroot="$cross_root/$cross_tripple/sysroot"
  export QEMU_LD_PREFIX="$sysroot"
  export QEMU_SET_ENV="LD_LIBRARY_PATH=$cross_root/lib:$sysroot"
  if ! command -v qemu-aarch64-static >/dev/null 2>&1 && ! command -v qemu-aarch64 >/dev/null 2>&1; then
    echo "qemu-aarch64 user-mode emulator not found; provisioning a static build..."
    local qemu_url="https://github.com/multiarch/qemu-user-static/releases/download/v7.2.0-1/qemu-aarch64-static"
    if ! { command -v curl >/dev/null 2>&1 \
           && sudo curl -fsSL "$qemu_url" -o /usr/bin/qemu-aarch64-static \
           && sudo chmod +x /usr/bin/qemu-aarch64-static; }; then
      echo "ERROR: could not provision qemu-aarch64. Install it manually, e.g.:"
      echo "       sudo apt-get install -y qemu-user-static"
      exit 1
    fi
  fi

  # Force static linking of readline/ncurses (mirrors the x64 ci-base-image) by removing the
  # unversioned .so symlinks from the sysroot, so the cross-built hived needs only glibc at runtime
  # (the minimal runtime image ships no readline/ncurses).
  local lib
  for lib in readline history ncurses curses; do
    rm -f "$sysroot/usr/lib/lib$lib.so" 2>/dev/null || sudo rm -f "$sysroot/usr/lib/lib$lib.so" 2>/dev/null || true
  done

  add_cmake_arg "-DCMAKE_TOOLCHAIN_FILE=$toolchain_file"

  # Use the preinstalled aarch64 vendor libs (RocksDB) when available, so the cross build skips the
  # ~2000-file RocksDB compile. The ci-base-image exports HIVE_AARCH64_VENDOR_DIR; --vendor-dir wins.
  local vendor_dir="${VENDOR_DIR_ARG:-${HIVE_AARCH64_VENDOR_DIR:-}}"
  if [[ -n "$vendor_dir" ]]; then
    echo "Using preinstalled aarch64 vendor libs at: $vendor_dir"
    add_cmake_arg "-DHIVE_USE_PREINSTALLED_VENDOR=ON"
    add_cmake_arg "-DHIVE_VENDOR_PREINSTALLED_DIR=$vendor_dir"
  fi
}

while [ $# -gt 0 ]; do
  case "$1" in
    --cmake-arg=*)
        arg="${1#*=}"
        add_cmake_arg "$arg"
        ;;
    --cross-root=*)
        CROSS_ROOT_ARG="${1#*=}"
        ;;
    --vendor-dir=*)
        VENDOR_DIR_ARG="${1#*=}"
        ;;
    --binary-dir=*)
        HIVED_BINARY_DIR="${1#*=}"
        ;;
    --source-dir=*)
        HIVED_SOURCE_DIR="${1#*=}"
        ;;
    --flat-binary-directory=*)
        HIVED_INSTALLATION_DIR="${1#*=}"
        ;;
    --clean-after-build)
        CLEAN_AFTER_BUILD="true"
        ;;
    --haf-build)
        HAF_BUILD="true"
        ;;
    --jobs=*)
        USER_JOBS="${1#*=}"
        ;;
    --help)
        print_help
        exit 0
        ;;
    -*)
        echo "ERROR: '$1' is not a valid option."
        echo
        print_help
        exit 1
        ;;
    *)
        break
        ;;
    esac
    shift
done

# When --cross-root is given, set up the entire aarch64 cross-compilation environment.
if [[ -n "$CROSS_ROOT_ARG" ]]; then
  setup_cross_environment "$CROSS_ROOT_ARG"
fi

# Determine number of parallel jobs
if [[ -n "$USER_JOBS" ]]; then
  if ! [[ "$USER_JOBS" =~ ^[1-9][0-9]*$ ]]; then
    echo "ERROR: --jobs requires a positive integer, got: '$USER_JOBS'"
    exit 1
  fi
  JOBS="$USER_JOBS"
  echo "Build will use $JOBS concurrent jobs (user-specified)..."
else
  JOBS=$(calculate_jobs)
  AVAILABLE_MEM_GB=$(awk '/MemAvailable/ {printf "%d", $2/1024/1024}' /proc/meminfo 2>/dev/null || echo "128")
  echo "Build will use $JOBS concurrent jobs (RAM: ${AVAILABLE_MEM_GB}GB)..."
fi

abs_src_dir=$(realpath -e --relative-base="$SCRIPTPATH" "$HIVED_SOURCE_DIR")
abs_build_dir=$(realpath -m --relative-base="$SCRIPTPATH" "$HIVED_BINARY_DIR")

# pwd
mkdir -vp "$abs_build_dir"
pushd "$abs_build_dir"
# pwd

echo "Building Hive!"

cmake -DCMAKE_BUILD_TYPE=Release -GNinja "${CMAKE_ARGS[@]}" "$abs_src_dir"
ninja -j "$JOBS" "$@"

if [[ "$CLEAN_AFTER_BUILD" == "true" ]]; then
    echo "Cleaning up after build..."
    # Patterns for find must be quoted so they're not expanded by Bash
    find . -name "*.o"  -type f -delete
    find . -name "*.a"  -type f -delete
fi

echo "Done!"

popd

if [[ -d "$HIVED_INSTALLATION_DIR" ]]; then

    # Move all the binaries to the $HIVED_INSTALLATION_DIR directory
    mv "$abs_build_dir/${HAF_BUILD:+"hive/"}programs/hived/hived" \
    "$abs_build_dir/${HAF_BUILD:+"hive/"}programs/cli_wallet/cli_wallet" \
    "$abs_build_dir/${HAF_BUILD:+"hive/"}programs/util/"* \
    "$HIVED_INSTALLATION_DIR/"

    rm -rf "$HIVED_INSTALLATION_DIR/CMakeFiles"

    if [[ -n "$(shopt -s nullglob; echo "$abs_build_dir/${HAF_BUILD:+"hive/"}programs/blockchain_converter/blockchain_converter"*)" ]]; then
        mv "$abs_build_dir/${HAF_BUILD:+"hive/"}programs/blockchain_converter/blockchain_converter"* \
            "$HIVED_INSTALLATION_DIR/"
    fi

    if [[ -n "$(shopt -s nullglob; echo "$abs_build_dir/${HAF_BUILD:+"hive/"}bin/"*)" ]]; then
        mv "$abs_build_dir/${HAF_BUILD:+"hive/"}bin/"* \
            "$HIVED_INSTALLATION_DIR/"
    fi

    if [[ -n "$(shopt -s nullglob; echo "$abs_build_dir/bin/"*)" ]]; then
        mv "$abs_build_dir/bin/"* \
            "$HIVED_INSTALLATION_DIR/"
    fi

    if [[ -n "$(shopt -s nullglob; echo "$abs_build_dir/tests/unit/"*)" ]]; then
        mv "$abs_build_dir/tests/unit/"* "$HIVED_INSTALLATION_DIR/"
    fi

    rm -rf "$HIVED_INSTALLATION_DIR/CMakeFiles" "$HIVED_INSTALLATION_DIR/cmake_install.cmake"

    # Copy sst_dump tools - check build directory first, then preinstalled path
    _sst_dump_found=false

    # Source build path (current behavior)
    if [[ -n "$(shopt -s nullglob; echo "$abs_build_dir/${HAF_BUILD:+"hive/"}libraries/vendor/rocksdb/tools/sst_dum"*)" ]]; then
        mv "$abs_build_dir/${HAF_BUILD:+"hive/"}libraries/vendor/rocksdb/tools/sst_dum"* \
            "$HIVED_INSTALLATION_DIR/"
        _sst_dump_found=true
    fi

    # Preinstalled path fallback (when using preinstalled RocksDB)
    if [[ "$_sst_dump_found" == "false" ]]; then
        _tools_dir_file="$abs_build_dir/${HAF_BUILD:+"hive/"}rocksdb_tools_dir.txt"
        if [[ -f "$_tools_dir_file" ]]; then
            _preinstalled_tools=$(cat "$_tools_dir_file")
            if [[ -n "$_preinstalled_tools" && -n "$(shopt -s nullglob; echo "$_preinstalled_tools/sst_dum"*)" ]]; then
                # Use cp, not mv — do not modify the preinstalled directory
                cp "$_preinstalled_tools/sst_dum"* "$HIVED_INSTALLATION_DIR/"
            fi
        fi
    fi

fi
