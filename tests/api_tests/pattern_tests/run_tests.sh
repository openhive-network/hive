#!/bin/bash

if [[ $1 == *":"* ]]; then
    export HIVEMIND_ADDRESS=${1%:*}
    export HIVEMIND_PORT=${1##*:}
else
    export HIVEMIND_ADDRESS=localhost
    export HIVEMIND_PORT=$1
fi
export TAVERN_DIR="$2/tests/api_tests/pattern_tests/"

default_testname_pat="not get_transaction_hex and (get_transaction or get_account_history or enum_virtual_ops or get_ops_in_block)"
if [[ "$3" == "direct_call_hafah" ]]; then
    export IS_DIRECT_CALL_HAFAH=TRUE
    TEST_NAMES="account_history_api_"
else
    export IS_DIRECT_CALL_HAFAH=FALSE
    TEST_NAMES=$default_testname_pat
fi

tox -e tavern --                                 \
    --tb=line                                    \
    -n 5                                         \
    --cache-clear                                \
    -p no:logging                                \
    --show-capture=log                           \
    --junitxml=results.xml                       \
    -W ignore::pytest.PytestDeprecationWarning   \
    -k "${TEST_NAMES}"
