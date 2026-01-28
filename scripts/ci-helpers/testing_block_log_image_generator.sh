#! /bin/bash

set -euo pipefail

GENERATOR_PATH=$1
GENERATOR_NAME=$2
TESTING_BLOCK_LOGS_DIR=$3
REGISTRY=$4
if [ -z "$REGISTRY" ]; then
  echo "Registry is not specified." >&2
  exit 1
fi

IMAGE_OUTPUT="--load"
for arg in "$@"; do
  if [ "$arg" == "--push" ]; then
    IMAGE_OUTPUT="--push"
    break
  fi
done

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
# shellcheck source=./docker_image_utils.sh
source "$SCRIPTPATH/docker_image_utils.sh"

IMGNAME=testing-block-logs
CHANGES=("$GENERATOR_PATH")
final_checksum=$(cat "${CHANGES[@]}" | sha256sum | tr -d '[:blank:] [=-=]')

tag=$GENERATOR_NAME-$final_checksum
img=$( build_image_name "$tag" "$REGISTRY" $IMGNAME )

image_exists=0
docker_image_exists "$img" image_exists

if [ "$image_exists" -eq 1 ];
then
  echo "Testing block log image is up to date. Skipping generation..."
else
  echo "${img} image is missing. Starting generation of testing block logs..."
  echo "Save block logs to directory: $TESTING_BLOCK_LOGS_DIR"
  export LOGURU_LEVEL=INFO

  ARGS=(--output-block-log-directory "$TESTING_BLOCK_LOGS_DIR/block_logs_for_testing")

  if [[ "$GENERATOR_NAME" == *"universal_block_log"* ]]; then
    UNIVERSAL_TYPE=$(echo "$GENERATOR_NAME" | cut -d'_' -f1-2)
    ARGS+=(--universal-block-log-type "$UNIVERSAL_TYPE")
  fi

  python3 "$GENERATOR_PATH" "${ARGS[@]}"

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
  docker buildx build \
      --progress="plain" \
      --tag "$img" \
      "${IMAGE_OUTPUT}" \
      --file Dockerfile .
  if [ "$IMAGE_OUTPUT" = "--push" ]; then
    echo "Image pushed: $img"
  else
    echo "Image built locally: $img"
  fi
fi

echo "$GENERATOR_NAME=$final_checksum" > "$TESTING_BLOCK_LOGS_DIR/checksums/${GENERATOR_NAME}.env"
echo "Writing env to: $TESTING_BLOCK_LOGS_DIR/checksums/${GENERATOR_NAME}.env"
ls -la "$TESTING_BLOCK_LOGS_DIR/checksums"
