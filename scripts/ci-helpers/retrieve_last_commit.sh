#! /bin/bash

P="${1}"
pushd "$P" >/dev/null 2>&1
# this list is used to detect changes affecting hived binaries, list might change in future
COMMIT=$(git log --pretty=format:"%H" -- libraries/ programs/ scripts/ docker/ Dockerfile cmake CMakeLists.txt | head -1)

popd >/dev/null 2>&1

echo "$COMMIT"
