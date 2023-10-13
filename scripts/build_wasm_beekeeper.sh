#! /bin/bash

set -xeuo pipefail

if [ $# -eq 0 ]; then
  echo "Performing a docker run..."
docker run \
  -it --rm \
  -v $(pwd)/../:/src \
  -u $(id -u):$(id -g) \
  registry.gitlab.syncad.com/hive/common-ci-configuration/emsdk:3.1.43 \
  /bin/bash /src/scripts/build_wasm_beekeeper.sh 1
else
  echo "Performing a build..."
  cd /src
  rm -rf ./programs/beekeeper/beekeeper_wasm/build
  mkdir -vp programs/beekeeper/beekeeper_wasm/build
  cd programs/beekeeper/beekeeper_wasm/build

  #-DBoost_DEBUG=TRUE -DBoost_VERBOSE=TRUE -DCMAKE_STATIC_LIBRARY_SUFFIX=".a;.bc"
  cmake \
    -DBoost_NO_WARN_NEW_VERSIONS=1 \
    -DBoost_USE_STATIC_RUNTIME=ON \
    -DCMAKE_TOOLCHAIN_FILE=/emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake -DCMAKE_BUILD_TYPE=Release -G "Unix Makefiles" -S /src/programs/beekeeper/beekeeper_wasm/ -B /src/programs/beekeeper/beekeeper_wasm/build/
  make -j8
fi

#  emcc -sFORCE_FILESYSTEM test.cpp -o test.js -lembind -lidbfs.js --emrun

# execute on host machine
#node helloworld.js
