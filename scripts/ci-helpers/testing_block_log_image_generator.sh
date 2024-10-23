#! /bin/bash

set -euo pipefail

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"

# shellcheck source=./docker_image_utils.sh
source "$SCRIPTPATH/docker_image_utils.sh"

submodule_path=$CI_PROJECT_DIR
REGISTRY=$CI_REGISTRY_IMAGE
REGISTRY_USER=$REGISTRY_USER
REGISTRY_PASSWORD=$REGISTRY_PASS
IMGNAME=testing-block-logs

echo "Attempting to get commit for: $submodule_path"

CHANGES=(tests/python/functional/util/testing_block_logs/generate_testing_block_logs.py)
commit=$("$SCRIPTPATH/retrieve_last_commit.sh" "${submodule_path}" "${CHANGES[@]}")
echo "commit with last source code changes is $commit"

pushd "${submodule_path}"
short_commit=$(git -c core.abbrev=8 rev-parse --short "$commit")
popd

prefix_tag="testing-block-log"
tag=$prefix_tag-$short_commit

img=$( build_image_name "$tag" "$REGISTRY" $IMGNAME )
_img_path=$( build_image_registry_path "$tag" "$REGISTRY" $IMGNAME )
_img_tag="$tag"

echo "$REGISTRY_PASSWORD" | docker login -u "$REGISTRY_USER" "$REGISTRY" --password-stdin

image_exists=0
docker_image_exists "$img" image_exists

if [ "$image_exists" -eq 1 ];
then
  echo "Testing block log image is up to date. Skipping generation..."
else
  echo "${img} image is missing. Starting generation of testing block logs..."
  echo "Save block logs to directory: $TESTING_BLOCK_LOGS_DIR"
  cd tests/python/functional/util/testing_block_logs
  python3 generate_testing_block_logs.py --output-block-log-directory="$TESTING_BLOCK_LOGS_DIR/block_logs_for_testing"
  echo "Block logs saved in: $TESTING_BLOCK_LOGS_DIR"

  echo "Build a Dockerfile"

  pwd
  cd "$TESTING_BLOCK_LOGS_DIR"
  pwd

  cat <<EOF > Dockerfile
FROM nginx:alpine3.20-slim
COPY block_logs_for_testing /usr/share/nginx/html/testing_block_logs
RUN sed -i "/index  index.html index.htm;/a \    autoindex on;" /etc/nginx/conf.d/default.conf
EXPOSE 80
EOF
  cat Dockerfile
  echo "Build docker image containing testing_block_logs"
  docker build -t "$img" .
  docker push "$img"
  echo "Created and push docker image with testing block logs: $img"
fi

echo "TEST_BLOCK_LOG_LATEST_VERSION_IMAGE=$img" > $CI_PROJECT_DIR/testing_block_log_latest_version.env
echo "TEST_BLOCK_LOG_LATEST_COMMIT_SHORT_SHA=$short_commit" > $CI_PROJECT_DIR/testing_block_log_latest_commit_short_sha.env
