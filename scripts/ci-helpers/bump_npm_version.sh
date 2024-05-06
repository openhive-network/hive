#!/usr/bin/env bash

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
PROJECT_DIR="${SCRIPTPATH}/../../programs/beekeeper/beekeeper_wasm"

# registry.npmjs.org/
REGISTRY_URL="${1:-gitlab.syncad.com/api/v4/packages/npm/}"
PUBLISH_TOKEN="${2:-}"
SCOPE="${3:-@hiveio}"

PROJECT_NAME="beekeeper"

git config --global --add safe.directory '*'

git fetch --tags

SHORT_HASH=$(git rev-parse --short HEAD)
CURRENT_BRANCH_IMPL=$(git branch -r --contains "${SHORT_HASH}")
if [ "${CURRENT_BRANCH_IMPL}" = "" ]; then
  CURRENT_BRANCH=$(git rev-parse --abbrev-ref HEAD)
else
  CURRENT_BRANCH="${CURRENT_BRANCH_IMPL#*/}"
fi
GIT_COMMIT_TIME=$(TZ=UTC0 git show --quiet --date='format-local:%Y%m%d%H%M%S' --format="%cd")
TAG_TIME=${GIT_COMMIT_TIME:2}
TAG=$(git tag --sort=-taggerdate | grep -Eo '[0-9]+\.[0-9]+\.[0-9]+(-.+)?' | tail -1)

echo "Preparing npm packge for ${CURRENT_BRANCH}@${TAG} (#${SHORT_HASH})"

if [ "${TAG}" = "" ]; then
  echo "Could not find a valid tag name for branch"
  exit 1
fi

DIST_TAG=""
NEW_VERSION=""

if [ "$CURRENT_BRANCH" = "master" ]; then
  DIST_TAG="latest"
  NEW_VERSION="${TAG}"
elif [ "$CURRENT_BRANCH" = "develop" ]; then
  DIST_TAG="stable"
  NEW_VERSION="${TAG}-stable.${TAG_TIME}"
else
  DIST_TAG="dev"
  NEW_VERSION="${TAG}-${TAG_TIME}"
fi

echo> "${PROJECT_DIR}/.npmrc"

if [ "$REGISTRY_URL" != "registry.npmjs.org/" ]; then
  echo "${SCOPE}:registry=https://${REGISTRY_URL}" >> "${PROJECT_DIR}/.npmrc"
fi

echo "//${REGISTRY_URL}:_authToken=\"${PUBLISH_TOKEN}\"" >> "${PROJECT_DIR}/.npmrc"

git checkout "${PROJECT_DIR}/package.json" # be sure we're on clean version

jq ".name = \"${SCOPE}/${PROJECT_NAME}\" | .version = \"$NEW_VERSION\" | .publishConfig.registry = \"https://${REGISTRY_URL}\" | .publishConfig.tag = \"${DIST_TAG}\"" "${PROJECT_DIR}/package.json" > "${PROJECT_DIR}/package.json.tmp"

mv "${PROJECT_DIR}/package.json.tmp" "${PROJECT_DIR}/package.json"

# Display detailed publish config data
jq -r '.name + "@" + .version + " (" + .publishConfig.tag + ") " + .publishConfig.registry' "${PROJECT_DIR}/package.json"
