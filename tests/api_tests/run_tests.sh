#!/bin/bash

if [[ $1 == *":"* ]]; then
    export HIVEMIND_ADDRESS=${1%:*}
    export HIVEMIND_PORT=${1##*:}
else
    export HIVEMIND_ADDRESS=localhost
    export HIVEMIND_PORT=$1
fi
export TAVERN_DIR="$2/tests/api_tests"
tox -e tavern -- --tb=long -W ignore::pytest.PytestDeprecationWarning --workers 1 --tests-per-worker auto -k account_history_api --junitxml=results.xml
