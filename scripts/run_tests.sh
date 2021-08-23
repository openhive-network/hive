#!/bin/bash

# this script will run tavern test versus hivemind instance at address and port
# specified in command line args.

# basic call is: ./run_tests.sh address port
# example: ./run_tests.sh 127.0.0.1 8080

# additionaly one can pass parameters to underlying pytest framework
# example: ./run_tests.sh 127.0.0.1 8080 -m failing
# above will run only tests marked as failing

# you can also specify tests from given file:
# example: ./run_tests.sh localhost 8080 test_database_api_patterns.tavern.yaml

# or combine all options
# ./run_tests.sh localhost 8080 test_database_api_patterns.tavern.yaml -m failing

function display_usage {
  echo "Usage: $0 hivemind_address:hivemind_port [test options]"
}

function check_port {
  re='^[0-9]+$'
  if ! [[ $1 =~ $re ]] ; then
    echo "Error: Port is not a number" >&2
    exit 1
  fi
}

function check_address {
  if [[ $1 -eq "localhost" ]] ; then
    return
  fi

  re='^(http(s?):\/\/)?((((www\.)?)+[a-zA-Z0-9\.\-\_]+(\.[a-zA-Z]{2,6})+)|(\b((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\b))(\/[a-zA-Z0-9\_\-\s\.\/\?\%\#\&\=]*)?$'
  if ! [[ $1 =~ $re ]] ; then
    echo "Error: Address is not valid url or ip address" >&2
    exit 1
  fi
}

if [ $# -lt 2 ]
then 
  display_usage
  exit 1
fi

if [[ ( $# == "--help") ||  $# == "-h" ]]
then
  display_usage
  exit 0
fi

#check_address $1
#check_port $2

#cd ..

set -e

pip3 install tox --user

export HIVEMIND_URL=$1
if [ -z "$TAVERN_DIR" ]
then
  export TAVERN_DIR="$(realpath ./tests/api_tests/tavern/)"
fi
echo "Attempting to start tests on hived instance listening on: $HIVEMIND_URL"

echo "Additional test options: ${@:2}"

tox -- -W ignore::pytest.PytestDeprecationWarning --workers auto --tests-per-worker auto -p no:logging ${@:2}
