#!/bin/bash

set -euo pipefail

REGISTRY_USER=${REGISTRY_USER:-gitlab-ci-token}
REGISTRY_PASS=${REGISTRY_PASS:-}
if [[ -z "${IMAGE_PATH:-}" ]]; then
  : "${CI_REGISTRY:?Missing CI_REGISTRY or IMAGE_PATH}"
  : "${CI_PROJECT_PATH:?Missing CI_PROJECT_PATH or IMAGE_PATH}"
  IMAGE_PATH="${CI_REGISTRY}/${CI_PROJECT_PATH}"
fi
DOCKER_IMAGE=${IMAGE_PATH}
DOCKER_SERVER=${DOCKER_SERVER:-${CI_REGISTRY:-}}

#PROJECT_ID=$( curl --fail --silent --request GET --url https://gitlab.syncad.com/api/v4/projects/hive%2Fhive  | jq .id )
if [[ -z "${CI_JOB_TOKEN:-}" && -z "${REGISTRY_PASS:-}" ]]; then
  echo "Missing CI_JOB_TOKEN or REGISTRY_PASS" >&2
  exit 1
fi
: "${IMAGE_TAG:?Missing IMAGE_TAG}"
PROJECT_ID=${CI_PROJECT_ID:?"Missing CI_PROJECT_ID"}
echo "Project ID: ${PROJECT_ID}"

gitlab_api_curl() {
  if [[ -n "${CI_JOB_TOKEN:-}" ]]; then
    curl --fail --silent --show-error --header "JOB-TOKEN: ${CI_JOB_TOKEN}" "$@" && return
  fi

  if [[ -n "${REGISTRY_PASS:-}" ]]; then
    curl --fail --silent --show-error --header "PRIVATE-TOKEN: ${REGISTRY_PASS}" "$@" && return
  fi

  return 1
}

REGISTRY_ID=$(
  gitlab_api_curl --request GET \
    --url "https://gitlab.syncad.com/api/v4/projects/${PROJECT_ID}/registry/repositories" \
    --header 'Accept: application/vnd.docker.distribution.manifest.v2+json' |
    jq -r --arg image_path "${IMAGE_PATH}" 'first(.[] | select(.location == $image_path) | .id) // empty'
)
echo "Repository ID: ${REGISTRY_ID}"

TST_REGISTRY_ID=${REGISTRY_ID:?"Missing a registry image matching to given path"}

echo "Deleting '${IMAGE_PATH}:${IMAGE_TAG}' from registry..."

gitlab_api_curl --request DELETE \
  "https://gitlab.syncad.com/api/v4/projects/${PROJECT_ID}/registry/repositories/${REGISTRY_ID}/tags/${IMAGE_TAG}"
