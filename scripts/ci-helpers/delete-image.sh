#! /bin/bash

set -ex

REGISTRY_USER=${REGISTRY_USER:-gitlab-ci-token}
REGISTRY_PASS=${REGISTRY_PASS:-${CI_JOB_TOKEN}}
DOCKER_IMAGE=${IMAGE_PATH:-${CI_REGISTRY}/${CI_PROJECT_PATH}}
DOCKER_SERVER=${DOCKER_SERVER:-${CI_REGISTRY}}

#PROJECT_ID=$( curl --fail --silent --request GET --url https://gitlab.syncad.com/api/v4/projects/hive%2Fhive  | jq .id )
PROJECT_ID=${CI_PROJECT_ID}
echo "Project ID: ${PROJECT_ID}"

REGISTRY_ID=$( curl --request GET --url https://gitlab.syncad.com/api/v4/projects/${PROJECT_ID}/registry/repositories   --header 'Accept: application/vnd.docker.distribution.manifest.v2+json' --header 'Authorization: Bearer' | jq -c ".[] | select (.location==\"${IMAGE_PATH}\") | .id" )
echo "Repository ID: ${REGISTRY_ID}"

TST_REGISTRY_ID=${REGISTRY_ID:?"Missing a registry image matching to given path"}

echo "Deleting '${IMAGE_PATH}:${IMAGE_TAG}' from registry..."

curl --request DELETE --header "PRIVATE-TOKEN: ${REGISTRY_PASS}" \
  "https://gitlab.syncad.com/api/v4/projects/${PROJECT_ID}/registry/repositories/${REGISTRY_ID}/tags/${IMAGE_TAG}"
