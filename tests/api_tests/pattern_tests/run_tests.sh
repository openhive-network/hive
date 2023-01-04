#!/bin/bash

export PYTHONPATH="$2/tests/test_tools/package"

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

tox -e tavern --                                 \
    --tb=line                                    \
    -n 8                                         \
    --cache-clear                                \
    -p no:logging                                \
    --show-capture=log                           \
    --junitxml=results.xml                       \
    -W ignore::pytest.PytestDeprecationWarning   \
    -W ignore::DeprecationWarning                \
    -k "${default_testsuite}"
