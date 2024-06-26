#!/bin/bash

set -e

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
PROJECT_DIR="${SCRIPTPATH}/../programs/beekeeper/beekeeper_wasm"

VERSION_PLACEHOLDER=$(jq -r ".version" "${PROJECT_DIR}/package.json")
REGISTRY_PLACEHOLDER=$(jq -r ".publishConfig.registry" "${PROJECT_DIR}/package.json")
DISTTAG_PLACEHOLDER=$(jq -r ".publishConfig.tag" "${PROJECT_DIR}/package.json")

STAGED_FILES=($(git diff --name-only --cached))

if [[ ! " ${STAGED_FILES[*]} " =~ " programs/beekeeper/beekeeper_wasm/package.json " ]];
then
  echo "package.json file is not staged for commit - skipping further checks..."
  exit 0
fi

# Verify each placeholder in the package.json file to block commits holding i.e. effective version (hash)
if [[ "${VERSION_PLACEHOLDER}" != "0.0.1-LastGitTagPlaceholder.GitHashPlaceholder" ]];
then
  echo "Version placeholder in package.json is broken - preventing commit."
  exit 1
fi

if [[ "${REGISTRY_PLACEHOLDER}" != "https://RegistryPlaceholder" ]];
then
  echo "Registry placeholder in package.json is broken - preventing commit."
  exit 2
fi

if [[ "${DISTTAG_PLACEHOLDER}" != "DistTagPlaceholder" ]];
then
  echo "Disttag placeholder in package.json is broken - preventing commit."
  exit 3
fi
