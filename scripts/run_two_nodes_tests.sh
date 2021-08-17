#!/bin/bash

# this script will run and compare tavern test versus hived instance at given addressess
# specified in command line args.

# you should be in tavern_run location: cd tests_api/hived/tavern_run/
# basic call is: ./run_two_nodes_tests.sh url1 url2 nodes_comparison/
# example: ./run_two_nodes_tests.sh https://api.hive.blog http://localhost:8080 nodes_comparison/

# additionaly one can pass parameters to underlying pytest framework
# example: ./run_two_nodes_tests.sh https://api.hive.blog nodes_comparison -m failing
# above will run only tests marked as failing

# you can also specify tests from given directory or file:
# example: ./run_two_nodes_tests.sh http://localhost:8080 https://api.hive.blog nodes_comparison/
# example: ./run_two_nodes_tests.sh http://localhost:8080 https://api.hive.blog nodes_comparison/test_two_nodes_account_create_operation.yaml

function display_usage {
  echo "Usage: $0 hived_url [test options]"
}

if [ $# -lt 1 ]
then 
  display_usage
  exit 1
fi

if [[ ( $# == "--help") ||  $# == "-h" ]]
then
  display_usage
  exit 0
fi

set -e

pip3 install tox --user

export FIRST_HIVEMIND_URL=$1
echo "Attempting to start tests on hived instance listening on: $FIRST_HIVEMIND_URL"

export SECOND_HIVEMIND_URL=$2
if [ -z "$TAVERN_DIR" ]
then
  export TAVERN_DIR="../tavern/"
fi
echo "Attempting to start tests on hived instance listening on: $SECOND_HIVEMIND_URL"

echo "Additional test options: ${@:3}"

tox -- -W ignore::pytest.PytestDeprecationWarning -n auto --durations=0 -v -p no:logging ${@:3}
