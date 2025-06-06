#!/bin/bash
set -euo pipefail

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
source "$SCRIPTPATH/docker_image_utils.sh"

ENV_FILE="$1"
if [ ! -s "$ENV_FILE" ]; then
  echo "ENV file '$ENV_FILE' is missing or empty." >&2
  exit 1
fi

REGISTRY="$2"
if [ -z "$REGISTRY" ]; then
  echo "Registry is not specified." >&2
  exit 1
fi

PROJECT_DIR="$3"
if [ -z "$PROJECT_DIR" ]; then
  echo "Project directory is not specified." >&2
  exit 1
fi

IMAGE_OUTPUT="--load"
for arg in "$@"; do
  if [ "$arg" == "--push" ]; then
    IMAGE_OUTPUT="--push"
    break
  fi
done

IMGNAME="testing-block-logs"
FINAL_PREFIX="combined"
FINAL_CHECKSUM=$(sha256sum "$ENV_FILE" | cut -d' ' -f1)
FINAL_TAG="$FINAL_PREFIX-$FINAL_CHECKSUM"
FINAL_IMAGE="$REGISTRY/$IMGNAME:$FINAL_TAG"

echo "TEST_BLOCK_LOG_LATEST_VERSION_IMAGE=$FINAL_IMAGE" > "$PROJECT_DIR/testing_block_log_latest_version.env"

image_exists=0
docker_image_exists "$FINAL_IMAGE" image_exists

if [ "$image_exists" -eq 1 ]; then
  echo "Image $FINAL_IMAGE already exists. Skipping build."
  exit 0
fi

echo "Generating simplified Dockerfile using direct external image references..."
cat <<EOF > Dockerfile
FROM alpine:3.18 AS final
RUN mkdir -p /tmp_combined_block_logs
EOF

while IFS='=' read -r NAME GENERATOR_CHECKSUM; do
  if [ -n "$NAME" ] && [ -n "$GENERATOR_CHECKSUM" ]; then
    IMAGE_TAG="$REGISTRY/$IMGNAME:$NAME-$GENERATOR_CHECKSUM"
    cat <<EOF >> Dockerfile
COPY --from=$IMAGE_TAG /usr/share/nginx/html/testing_block_logs /tmp_combined_block_logs/$NAME
EOF
  fi
done < "$ENV_FILE"

cat <<EOF >> Dockerfile
FROM nginx:alpine3.20-slim
COPY --from=final /tmp_combined_block_logs /usr/share/nginx/html/testing_block_logs
RUN sed -i "/index  index.html index.htm;/a \    autoindex on;" /etc/nginx/conf.d/default.conf
EXPOSE 80
EOF

echo "Dockerfile generated:"
cat Dockerfile

echo "Building combined image: $FINAL_IMAGE"
docker buildx build \
    --progress="plain" \
    --tag "$FINAL_IMAGE" \
    "${IMAGE_OUTPUT}" \
    --file Dockerfile .

if [ "$IMAGE_OUTPUT" = "--push" ]; then
  echo "Combined testing-block-logs image pushed: $FINAL_IMAGE"
else
  echo "Combined testing-block-logs image built locally: $FINAL_IMAGE"
fi
