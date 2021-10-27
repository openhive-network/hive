#!/bin/bash

#pip3 install tox
#pip3 install tavern

export HIVEMIND_ADDRESS=localhost
export HIVEMIND_PORT=8090
export TAVERN_DIR='/home/dev/Patter_tests/src/hive/tests/api_tests' 
tox -e tavern -- -W ignore::pytest.PytestDeprecationWarning --workers auto --tests-per-worker auto -k account_history_api_patterns --junitxml=results.xml
