#!/bin/bash

if [[ $1 == *":"* ]]; then
    export HIVEMIND_ADDRESS=${1%:*}
    export HIVEMIND_PORT=${1##*:}
else
    export HIVEMIND_ADDRESS=localhost
    export HIVEMIND_PORT=$1
fi
export TAVERN_DIR="$2/tests/api_tests/pattern_tests/"

if [ -z "$3" ]; then
    TEST_NAMES='not get_transaction_hex and (get_transaction or get_account_history or enum_virtual_ops or get_ops_in_block)'
else
    TEST_NAMES=$3
    ./gen_new_style_calls.py --tavern-dir=$TAVERN_DIR
fi

tox -e tavern -- --tb=long -W ignore::pytest.PytestDeprecationWarning --workers 5 --tests-per-worker auto -k $TEST_NAMES --junitxml=results.xml

#rm -rf $TAVERN_DIR/tavern/hafah_api_negative
#rm -rf $TAVERN_DIR/tavern/hafah_api_patterns