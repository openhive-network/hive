#!/bin/bash

# this script will run tavern test versus hived instance at given address
# specified in command line args.

# you should be in tavern_run location: cd tests_api/hived/tavern_run/
# basic call is: ./run_tests.sh url
# example: ./run_tests.sh https://api.hive.blog

# additionaly one can pass parameters to underlying pytest framework
# example: ./run_tests.sh https://api.hive.blog -m failing
# above will run only tests marked as failing

# you can also specify tests from given directory or file:
# example: ./run_tests.sh http://localhost:8080 account_history_api_patterns/
# example: ./run_tests.sh http://localhost:8080 account_history_api_patterns/get_transaction.tavern.yaml

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

export HIVEMIND_URL=$1
if [ -z "$TAVERN_DIR" ]
then
  export TAVERN_DIR="hive/tests/api_tests/tavern"
fi
echo "Attempting to start tests on hived instance listening on: $HIVEMIND_URL"

echo "Additional test options: ${@:2}"

tox -- -W ignore::pytest.PytestDeprecationWarning --workers auto --tests-per-worker auto -p no:logging ${@:2}
