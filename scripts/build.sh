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
  --clean-after-build      Remove compiled files after build
  --haf-build              Set if build is called from HAF
  --help                   Display this help screen and exit.

EOF
}

HIVED_BINARY_DIR="../build"
HIVED_SOURCE_DIR="."
HIVED_INSTALLATION_DIR=""
CLEAN_AFTER_BUILD="false"
HAF_BUILD=""

CMAKE_ARGS=()

JOBS=$(nproc)
JOBS=$(( JOBS > 10 ? 10 : JOBS ))
echo "Build will use $JOBS concurrent jobs..."

add_cmake_arg () {
  CMAKE_ARGS+=("$1")
}

while [ $# -gt 0 ]; do
  case "$1" in
    --cmake-arg=*)
        arg="${1#*=}"
        add_cmake_arg "$arg"
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

abs_src_dir=$(realpath -e --relative-base="$SCRIPTPATH" "$HIVED_SOURCE_DIR")
abs_build_dir=$(realpath -m --relative-base="$SCRIPTPATH" "$HIVED_BINARY_DIR")

# pwd
mkdir -vp "$abs_build_dir"
pushd "$abs_build_dir"
# pwd

echo "Building Hive!"

cmake -DCMAKE_BUILD_TYPE=Release -GNinja "${CMAKE_ARGS[@]}" "$abs_src_dir"
ninja -j"$JOBS" "$@"

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
    sudo mv "$abs_build_dir/${HAF_BUILD:+"hive/"}programs/hived/hived" \
    "$abs_build_dir/${HAF_BUILD:+"hive/"}programs/cli_wallet/cli_wallet" \
    "$abs_build_dir/${HAF_BUILD:+"hive/"}programs/util/"* \
    "$HIVED_INSTALLATION_DIR/"

    sudo rm -rf "$HIVED_INSTALLATION_DIR/CMakeFiles"

    if [[ -f "$abs_build_dir/${HAF_BUILD:+"hive/"}programs/beekeeper/beekeeper/beekeeper" ]]; then
        sudo mv "$abs_build_dir/${HAF_BUILD:+"hive/"}programs/beekeeper/beekeeper/beekeeper" \
            "$HIVED_INSTALLATION_DIR/"
    fi

    if [[ -n "$(shopt -s nullglob; echo "$abs_build_dir/${HAF_BUILD:+"hive/"}programs/blockchain_converter/blockchain_converter"*)" ]]; then
        sudo mv "$abs_build_dir/${HAF_BUILD:+"hive/"}programs/blockchain_converter/blockchain_converter"* \
            "$HIVED_INSTALLATION_DIR/"
    fi

    if [[ -n "$(shopt -s nullglob; echo "$abs_build_dir/${HAF_BUILD:+"hive/"}bin/"*)" ]]; then
        sudo mv "$abs_build_dir/${HAF_BUILD:+"hive/"}bin/"* \
            "$HIVED_INSTALLATION_DIR/"
    fi

    if [[ -n "$(shopt -s nullglob; echo "$abs_build_dir/bin/"*)" ]]; then
        sudo mv "$abs_build_dir/bin/"* \
            "$HIVED_INSTALLATION_DIR/"
    fi

    if [[ -n "$(shopt -s nullglob; echo "$abs_build_dir/tests/unit/"*)" ]]; then
        sudo mv "$abs_build_dir/tests/unit/"* "$HIVED_INSTALLATION_DIR/"
    fi

    sudo rm -rf "$HIVED_INSTALLATION_DIR/CMakeFiles" "$HIVED_INSTALLATION_DIR/cmake_install.cmake"

    if [[ -n "$(shopt -s nullglob; echo "$abs_build_dir/${HAF_BUILD:+"hive/"}libraries/vendor/rocksdb/tools/sst_dum"*)" ]]; then
        sudo mv "$abs_build_dir/${HAF_BUILD:+"hive/"}libraries/vendor/rocksdb/tools/sst_dum"* \
            "$HIVED_INSTALLATION_DIR/"
    fi

fi
