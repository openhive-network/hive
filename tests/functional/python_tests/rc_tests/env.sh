if [ -z "${HIVE_BUILD_ROOT_PATH}" ]; then
    export HIVE_BUILD_ROOT_PATH=$(git rev-parse --show-toplevel)/build
fi
if [ -z "${PYTHONPATH}" ]; then
    export PYTHONPATH=$(git rev-parse --show-toplevel)/tests/test_tools/package
fi
