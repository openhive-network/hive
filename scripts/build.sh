#! /bin/bash

set -euo pipefail

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"

LOG_FILE=build.log

source "$SCRIPTPATH/common.sh"

log_exec_params "$@"

#Script purpose is to build all (or selected) targets in the Hived project.

print_help () {
    echo "Usage: $0 [OPTION[=VALUE]]... [target]..."
    echo
    echo "Allows to build HAF sources "
    echo "  --source-dir=DIRECTORY_PATH"
    echo "                       Allows to specify a directory containing a Hived source tree."
    echo "  --binary-dir=DIRECTORY_PATH"
    echo "                       Allows to specify a directory to store a build output (Hived binaries)."
    echo "                       Usually it is a \`build\` subdirectory in the Hived source tree."
    echo "  --cmake-arg=ARG      Allows to specify additional arguments to the CMake tool spawn"
    echo "  --help               Display this help screen and exit"
    echo
}

HIVED_BINARY_DIR="../build"
HIVED_SOURCE_DIR="."
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
    --help)
        print_help
        exit 0
        ;;
    -*)
        echo "ERROR: '$1' is not a valid option"
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

abs_src_dir=`realpath -e --relative-base="$SCRIPTPATH" "$HIVED_SOURCE_DIR"`
abs_build_dir=`realpath -m --relative-base="$SCRIPTPATH" "$HIVED_BINARY_DIR"`

pwd
mkdir -vp "$abs_build_dir"
pushd "$abs_build_dir"
pwd
cmake -DCMAKE_BUILD_TYPE=Release -GNinja "${CMAKE_ARGS[@]}" "$abs_src_dir"

ninja "$@"

popd

