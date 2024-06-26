#!/bin/bash

set -e

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
PROJECT_DIR="${SCRIPTPATH}/../programs/beekeeper/beekeeper_wasm"

REPO_URL="${1:?Missing repository url}"
REVISION_INFO="${2:?Missing argument pointing git revision}"

DOCUMENTATION_URL=${3}

OUTPUT_DIR=${4:-dist/docs}

INPUT_FILE=src/web.ts

# When using TypeScript, we are restricted to a specific typedoc and typedoc-plugin-markdown versions
# https://typedoc.org/guides/installation/#requirements
pushd "${PROJECT_DIR}"

mkdir -vp "${OUTPUT_DIR}"

pnpm exec typedoc --includeVersion \
  --sourceLinkTemplate "${REPO_URL}/-/blob/{gitRevision}/{path}#L{line}" \
  --gitRevision "${REVISION_INFO}" \
  --readme README.md \
  --plugin typedoc-plugin-markdown \
  --plugin typedoc-gitlab-wiki-theme \
  --tsconfig tsconfig.json \
  --out "${OUTPUT_DIR}" \
  "${INPUT_FILE}"

  if [[ -n "${DOCUMENTATION_URL}" ]]; then
    echo "Attempting to replace generated documentation url placeholder: ${DOCUMENTATION_URL}"
    sed -i "s<\${GEN_DOC_URL}<${DOCUMENTATION_URL}<g" README.md
  fi

popd

