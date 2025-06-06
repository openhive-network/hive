#! /bin/bash

set -euo pipefail

GENERATOR_PATH=$1
echo "$GENERATOR_PATH"
GENERATOR_NAME=$2
echo "$GENERATOR_NAME"
TESTING_BLOCK_LOGS_DIR=$3
echo "$TESTING_BLOCK_LOGS_DIR"

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"

# shellcheck source=./docker_image_utils.sh
source "$SCRIPTPATH/docker_image_utils.sh"

submodule_path=$CI_PROJECT_DIR
REGISTRY=$CI_REGISTRY_IMAGE
REGISTRY_USER=$REGISTRY_USER
REGISTRY_PASSWORD=$REGISTRY_PASS
IMGNAME=testing-block-logs

echo "Attempting to get commit for: $submodule_path"

CHANGES=("$GENERATOR_PATH")
final_checksum=$(cat "${CHANGES[@]}" | sha256sum | tr -d '[:blank:] [=-=]')
commit=$("$SCRIPTPATH/retrieve_last_commit.sh" "${submodule_path}" "${CHANGES[@]}")
echo "commit with last source code changes is $commit"

pushd "${submodule_path}"
short_commit=$(git -c core.abbrev=8 rev-parse --short "$commit")
popd

tag=$GENERATOR_NAME-$final_checksum

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
  python3 "$GENERATOR_PATH" "$OUTPUT_DIR_PARAMETER"
  ls -R "$TESTING_BLOCK_LOGS_DIR"

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

echo "$GENERATOR_NAME=$final_checksum" > "$TESTING_BLOCK_LOGS_DIR/checksums/${GENERATOR_NAME}.env"
echo "Writing env to: $TESTING_BLOCK_LOGS_DIR/checksums/${GENERATOR_NAME}.env"
ls -la "$TESTING_BLOCK_LOGS_DIR/checksums"
