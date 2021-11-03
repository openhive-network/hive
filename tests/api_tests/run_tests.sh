#!/bin/bash
#pip3 install tox
#pip3 install tavern

export HIVEMIND_ADDRESS=localhost
export HIVEMIND_PORT=$1
export TAVERN_DIR="$2/tests/api_tests"
pushd $2
tox -e tavern -- -W ignore::pytest.PytestDeprecationWarning --workers auto --tests-per-worker auto -k account_history_api_patterns --junitxml=results.xml
popd
