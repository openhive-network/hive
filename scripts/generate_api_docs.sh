#!/bin/bash

set -e

UNSTAGED_FILES=($(git diff --name-only))
STAGED_FILES=($(git diff --name-only --cached))

if   [[ " ${UNSTAGED_FILES[*]} " =~ " programs/beekeeper/beekeeper_wasm/src/interfaces.ts " ]] \
  || [[ " ${STAGED_FILES[*]} "   =~ " programs/beekeeper/beekeeper_wasm/src/interfaces.ts " ]];
then
echo "Beekeeper interfaces.ts file changes detected. Generating documentation"

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
PROJECT_DIR="${SCRIPTPATH}/../programs/beekeeper/beekeeper_wasm"

# When using TypeScript 4.4.4, we are restricted to a specific typedoc and typedoc-plugin-markdown versions
# https://typedoc.org/guides/installation/#requirements
pushd "${PROJECT_DIR}"
mkdir -vp build
pnpm exec typedoc --plugin typedoc-plugin-markdown --theme markdown --excludeInternal --hideBreadcrumbs --hideInPageTOC --out build/docs src/index.ts
mv build/docs/modules.md build/docs/_modules.md
rm build/docs/README.md
pnpm exec concat-md --decrease-title-levels build/docs > api.md
popd

UNSTAGED_FILES=($(git diff --name-only))

if [[ " ${UNSTAGED_FILES[*]} " =~ " programs/beekeeper/beekeeper_wasm/api.md " ]];
then
  echo "api.md file changes detected. Stage interface documentation changes before commit"
  exit 1
fi

echo "No unstaged changes in the documentation"

else
echo "No Beekeeper interfaces.ts changes detected. Omitting generating the documentation"
fi