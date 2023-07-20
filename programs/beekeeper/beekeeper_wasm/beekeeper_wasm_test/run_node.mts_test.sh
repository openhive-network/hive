#! /bin/bash

set -xeuo pipefail

if [ $# -eq 0 ]; then
  echo "Performing a docker run"
docker run \
  -it --rm \
  -v $(pwd)/../../../../:/src \
  -u $(id -u):$(id -g) \
  registry.gitlab.syncad.com/hive/common-ci-configuration/emsdk:3.1.43 \
  /bin/bash /src/programs/beekeeper/beekeeper_wasm/beekeeper_wasm_test/run_node.mts_test.sh 1
else
  echo "Attempting to start a NodeJS test"
  cd /src/programs/beekeeper/beekeeper_wasm/beekeeper_wasm_test

  node run_node.mjs

fi

#  emcc -sFORCE_FILESYSTEM test.cpp -o test.js -lembind -lidbfs.js --emrun 

# execute on host machine
#node helloworld.js