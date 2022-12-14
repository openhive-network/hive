#! /bin/bash
set -xeuo pipefail

P="${1}"
pushd "$P" >/dev/null 2>&1
i=0;
until [[ -n $(git show HEAD~${i} --numstat -- libraries/ programs/ scripts/ docker/ Dockerfile 2>&1 ) ]]
do
    ((i=i+1))
done

commit=$( git rev-parse HEAD~${i} )

popd >/dev/null 2>&1

echo "$commit"
