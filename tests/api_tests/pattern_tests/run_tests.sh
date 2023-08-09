#!/bin/bash

if [[ $1 == *":"* ]]; then
    export HIVEMIND_ADDRESS=${1%:*}
    export HIVEMIND_PORT=${1##*:}
else
    export HIVEMIND_ADDRESS=localhost
    export HIVEMIND_PORT=$1
fi

export TAVERN_DIR="$2/tests/api_tests/pattern_tests/"
default_testsuite=${3:?"Test suite must be specified as 3-rd script argument"}
export IS_DIRECT_CALL_HAFAH=${4:-FALSE}

pushd "$TAVERN_DIR"
pytest -n auto                                   \
    --cache-clear                                \
    --junitxml=results.xml                       \
    -k "${default_testsuite}" tavern/
popd
