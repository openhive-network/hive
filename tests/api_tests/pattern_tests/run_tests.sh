#!/bin/bash

if [[ $1 == *":"* ]]; then
    export HIVEMIND_ADDRESS=${1%:*}
    export HIVEMIND_PORT=${1##*:}
else
    export HIVEMIND_ADDRESS=localhost
    export HIVEMIND_PORT=$1
fi
export TAVERN_DIR="$2/tests/api_tests/pattern_tests/"
tox -e tavern --                                 \
    --tb=line                                    \
    --workers 5                                  \
    --cache-clear                                \
    -p no:logging                                \
    --show-capture=log                           \
    --junitxml=results.xml                       \
    -W ignore::pytest.PytestDeprecationWarning   \
    --tests-per-worker auto -k 'not get_transaction_hex and (get_transaction or get_account_history or enum_virtual_ops or get_ops_in_block)'
