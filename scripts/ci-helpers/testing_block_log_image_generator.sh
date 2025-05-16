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

CHANGES=(
  tests/python/functional/util/testing_block_logs/generate_testing_block_logs.py
  # tests/python/functional/comment_cashout_tests/block_log/generate_block_log.py
  tests/python/functional/datagen_tests/recalculation_proposal_vote_tests/block_log/generate_block_log.py
  # tests/python/functional/datagen_tests/recurrent_transfer_tests/block_logs/block_log_containing_many_to_one_recurrent_transfers/generate_block_log.py
  # tests/python/functional/datagen_tests/recurrent_transfer_tests/block_logs/block_log_recurrent_transfer_everyone_to_everyone/generate_block_log.py
  # tests/python/functional/datagen_tests/transaction_status_api_tests/block_log/generate_block_log.py
)
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
  export LOGURU_LEVEL=INFO
  OUTPUT_DIR_PARAMETER=--output-block-log-directory="$TESTING_BLOCK_LOGS_DIR/block_logs_for_testing"
  pushd tests/python/functional/util/testing_block_logs
  python3 generate_testing_block_logs.py $OUTPUT_DIR_PARAMETER
  popd
  # pushd tests/python/functional/comment_cashout_tests/block_log
  # python3 generate_block_log.py $OUTPUT_DIR_PARAMETER
  # popd
  pushd tests/python/functional/datagen_tests/recalculation_proposal_vote_tests/block_log
  python3 generate_block_log.py $OUTPUT_DIR_PARAMETER
  popd
  # pushd tests/python/functional/datagen_tests/recurrent_transfer_tests/block_logs/block_log_containing_many_to_one_recurrent_transfers
  # python3 generate_block_log.py $OUTPUT_DIR_PARAMETER
  # popd
  # pushd tests/python/functional/datagen_tests/recurrent_transfer_tests/block_logs/block_log_recurrent_transfer_everyone_to_everyone
  # python3 generate_block_log.py $OUTPUT_DIR_PARAMETER
  # popd
  # pushd tests/python/functional/datagen_tests/transaction_status_api_tests/block_log
  # python3 generate_block_log.py $OUTPUT_DIR_PARAMETER
  # popd
  echo "Block logs saved in: $TESTING_BLOCK_LOGS_DIR"
  #tree $TESTING_BLOCK_LOGS_DIR
  ls -R $TESTING_BLOCK_LOGS_DIR

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
