#!/bin/bash
set -euo pipefail

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
source "$SCRIPTPATH/docker_image_utils.sh"

ENV_FILE="$1"
if [ ! -s "$ENV_FILE" ]; then
  echo "ENV file '$ENV_FILE' is missing or empty." >&2
  exit 1
fi

REGISTRY=$CI_REGISTRY_IMAGE
REGISTRY_USER=$REGISTRY_USER
REGISTRY_PASSWORD=$REGISTRY_PASS
IMGNAME="testing-block-logs"
FINAL_PREFIX="combined"
FINAL_CHECKSUM=$(sha256sum "$ENV_FILE" | cut -d' ' -f1)
FINAL_TAG="$FINAL_PREFIX-$FINAL_CHECKSUM"
FINAL_IMAGE="$REGISTRY/$IMGNAME:$FINAL_TAG"

echo "TEST_BLOCK_LOG_LATEST_VERSION_IMAGE=$FINAL_IMAGE" > "$CI_PROJECT_DIR/testing_block_log_latest_version.env"

image_exists=0
docker_image_exists "$FINAL_IMAGE" image_exists

if [ "$image_exists" -eq 1 ]; then
  echo "Image $FINAL_IMAGE already exists. Skipping build."
  exit 0
fi

echo "$REGISTRY_PASSWORD" | docker login -u "$REGISTRY_USER" "$CI_REGISTRY" --password-stdin

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
docker build -t "$FINAL_IMAGE" .

echo "Pushing combined image: $FINAL_IMAGE"
docker push "$FINAL_IMAGE"

echo "Combined testing-block-logs image pushed: $FINAL_IMAGE"
