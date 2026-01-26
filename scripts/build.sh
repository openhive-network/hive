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

# Calculate parallel jobs: default 20, scale down for systems with less RAM
JOBS=$(nproc)
JOBS=$(( JOBS > 20 ? 20 : JOBS ))

# Memory-aware scaling: ~6GB per compile job
AVAILABLE_MEM_GB=$(awk '/MemAvailable/ {printf "%d", $2/1024/1024}' /proc/meminfo 2>/dev/null || echo "128")
MEM_BASED_JOBS=$(( AVAILABLE_MEM_GB / 6 ))
MEM_BASED_JOBS=$(( MEM_BASED_JOBS < 1 ? 1 : MEM_BASED_JOBS ))
if [[ $MEM_BASED_JOBS -lt $JOBS ]]; then
  JOBS=$MEM_BASED_JOBS
fi

echo "Build will use $JOBS concurrent jobs (RAM: ${AVAILABLE_MEM_GB}GB)..."

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
    sudo mv "$abs_build_dir/${HAF_BUILD:+"hive/"}programs/hived/hived" \
    "$abs_build_dir/${HAF_BUILD:+"hive/"}programs/cli_wallet/cli_wallet" \
    "$abs_build_dir/${HAF_BUILD:+"hive/"}programs/util/"* \
    "$HIVED_INSTALLATION_DIR/"

    sudo rm -rf "$HIVED_INSTALLATION_DIR/CMakeFiles"

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

    # Check for unit test binaries and copy them along with CTest configuration
    echo "Checking for unit test binaries in $abs_build_dir/tests/unit/..."
    if [[ -d "$abs_build_dir/tests/unit" ]]; then
        echo "Contents of $abs_build_dir/tests/unit/:"
        ls -la "$abs_build_dir/tests/unit/" || true
    fi

    if [[ -n "$(shopt -s nullglob; echo "$abs_build_dir/tests/unit/"*)" ]]; then
        echo "Copying unit test files to $HIVED_INSTALLATION_DIR/..."
        sudo mv "$abs_build_dir/tests/unit/"* "$HIVED_INSTALLATION_DIR/"

        # Copy BoostTestAddTests.cmake helper from source
        if [[ -f "$abs_src_dir/tests/unit/BoostTestAddTests.cmake" ]]; then
            echo "Copying BoostTestAddTests.cmake..."
            sudo cp "$abs_src_dir/tests/unit/BoostTestAddTests.cmake" "$HIVED_INSTALLATION_DIR/"
        else
            echo "WARNING: BoostTestAddTests.cmake not found at $abs_src_dir/tests/unit/"
        fi
        
        # Create root CTestTestfile.cmake for flattened directory structure
        # The generated *_include.cmake files have hardcoded build paths, so we
        # call BoostTestAddTests.cmake directly with the correct runtime paths
        sudo tee "$HIVED_INSTALLATION_DIR/CTestTestfile.cmake" > /dev/null <<'EOF'
# Autogenerated CTestTestfile for flattened binary directory
set(CMAKE_CURRENT_LIST_DIR "${CMAKE_CURRENT_LIST_DIR}")

# Discover chain_test tests
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/chain_test")
  if("${CMAKE_CURRENT_LIST_DIR}/chain_test" IS_NEWER_THAN "${CMAKE_CURRENT_LIST_DIR}/chain_test[1]_tests.cmake")
    include("${CMAKE_CURRENT_LIST_DIR}/BoostTestAddTests.cmake")
    boosttest_discover_tests_impl(
      TEST_TARGET "chain_test"
      TEST_EXECUTABLE "${CMAKE_CURRENT_LIST_DIR}/chain_test"
      TEST_EXECUTOR ""
      TEST_WORKING_DIR "${CMAKE_CURRENT_LIST_DIR}"
      TEST_EXTRA_ARGS ""
      TEST_PROPERTIES ""
      TEST_PREFIX "unit/chain_test-"
      TEST_SUFFIX ""
      TEST_NAME_SEPARATOR "/"
      TEST_LIST "chain_test_TESTS"
      TEST_SKIP_DISABLED "FALSE"
      CTEST_FILE "${CMAKE_CURRENT_LIST_DIR}/chain_test[1]_tests.cmake"
      TEST_DISCOVERY_TIMEOUT "60"
    )
  endif()
  include("${CMAKE_CURRENT_LIST_DIR}/chain_test[1]_tests.cmake")
endif()

# Discover plugin_test tests
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/plugin_test")
  if("${CMAKE_CURRENT_LIST_DIR}/plugin_test" IS_NEWER_THAN "${CMAKE_CURRENT_LIST_DIR}/plugin_test[1]_tests.cmake")
    include("${CMAKE_CURRENT_LIST_DIR}/BoostTestAddTests.cmake")
    boosttest_discover_tests_impl(
      TEST_TARGET "plugin_test"
      TEST_EXECUTABLE "${CMAKE_CURRENT_LIST_DIR}/plugin_test"
      TEST_EXECUTOR ""
      TEST_WORKING_DIR "${CMAKE_CURRENT_LIST_DIR}"
      TEST_EXTRA_ARGS ""
      TEST_PROPERTIES ""
      TEST_PREFIX "unit/plugin_test-"
      TEST_SUFFIX ""
      TEST_NAME_SEPARATOR "/"
      TEST_LIST "plugin_test_TESTS"
      TEST_SKIP_DISABLED "FALSE"
      CTEST_FILE "${CMAKE_CURRENT_LIST_DIR}/plugin_test[1]_tests.cmake"
      TEST_DISCOVERY_TIMEOUT "60"
    )
  endif()
  include("${CMAKE_CURRENT_LIST_DIR}/plugin_test[1]_tests.cmake")
endif()
EOF
        # Verify CTest files were created
        echo "Verifying CTest configuration files..."
        if [[ -f "$HIVED_INSTALLATION_DIR/chain_test" ]]; then
            echo "  chain_test: OK"
        else
            echo "  WARNING: chain_test executable not found!"
        fi
        if [[ -f "$HIVED_INSTALLATION_DIR/CTestTestfile.cmake" ]]; then
            echo "  CTestTestfile.cmake: OK"
        else
            echo "  WARNING: CTestTestfile.cmake not found!"
        fi
        if [[ -f "$HIVED_INSTALLATION_DIR/BoostTestAddTests.cmake" ]]; then
            echo "  BoostTestAddTests.cmake: OK"
        else
            echo "  WARNING: BoostTestAddTests.cmake not found!"
        fi
    else
        echo "WARNING: No unit test files found in $abs_build_dir/tests/unit/"
        echo "Unit tests will NOT be available. Is BUILD_HIVE_TESTNET=ON?"
    fi

    sudo rm -rf "$HIVED_INSTALLATION_DIR/CMakeFiles" "$HIVED_INSTALLATION_DIR/cmake_install.cmake"

    if [[ -n "$(shopt -s nullglob; echo "$abs_build_dir/${HAF_BUILD:+"hive/"}libraries/vendor/rocksdb/tools/sst_dum"*)" ]]; then
        sudo mv "$abs_build_dir/${HAF_BUILD:+"hive/"}libraries/vendor/rocksdb/tools/sst_dum"* \
            "$HIVED_INSTALLATION_DIR/"
    fi

fi