#! /bin/bash

set -euo pipefail

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"

# shellcheck source=./docker_image_utils.sh
source "$SCRIPTPATH/docker_image_utils.sh"

submodule_path=$CI_PROJECT_DIR
REGISTRY=$CI_REGISTRY_IMAGE
REGISTRY_USER=$REGISTRY_USER
REGISTRY_PASSWORD=$REGISTRY_PASS
IMGNAME=/universal-block-logs

echo "Attempting to get commit for: $submodule_path"

CHANGES=(
  "tests/python/functional/util/universal_block_logs/generate_universal_block_logs.py"
  "scripts/ci-helpers/universal_block_log_image_generator.sh"
)

final_checksum=$(cat "${CHANGES[@]}" | sha256sum | tr -d '[:blank:] [=-=]')
commit=$("$SCRIPTPATH/retrieve_last_commit.sh" "${submodule_path}" "${CHANGES[@]}")
echo "commit with last source code changes is $commit"

pushd "${submodule_path}"
short_commit=$(git -c core.abbrev=8 rev-parse --short "$commit")
popd

prefix_tag="universal-block-log"
tag=$prefix_tag-$final_checksum

img=$( build_image_name $IMGNAME "$tag" "$REGISTRY" )
img_path=$( build_image_registry_path $IMGNAME "$tag" "$REGISTRY" )
img_tag=$( build_image_registry_tag $IMGNAME "$tag" "$REGISTRY" )

echo "$REGISTRY_PASSWORD" | docker login -u "$REGISTRY_USER" "$REGISTRY" --password-stdin

image_exists=0
docker_image_exists $IMGNAME "$tag" "$REGISTRY" image_exists

if [ "$image_exists" -eq 1 ];
then
  echo "Universal block log image is up to date. Skipping generation..."
else
  echo "${img} image is missing. Start generate universal block logs..."
  echo "Save block logs to directory: $UNIVERSAL_BLOCK_LOGS_DIR"
  cd tests/python/functional/util/universal_block_logs
  python3 generate_universal_block_logs.py --output-block-log-directory="$UNIVERSAL_BLOCK_LOGS_DIR"
  echo "Block logs saved in: $UNIVERSAL_BLOCK_LOGS_DIR"

  # Note that checksum generation (and check in jobs which get the block logs from docker image)
  # has been removed when the image created below has been turned into http server thus eliminating
  # the intermediate step of runner-based cache directories.
  #
  # checksum=$(find $UNIVERSAL_BLOCK_LOGS_DIR -type f | sort | xargs cat | md5sum |cut -d ' ' -f 1)
  # echo "Checksum of the generated universal block logs: $checksum"

  echo "Build a Dockerfile"

  pwd
  cd $UNIVERSAL_BLOCK_LOGS_DIR
  pwd

  cat <<EOF > Dockerfile
FROM nginx:alpine3.20-slim
COPY block_log_open_sign /usr/share/nginx/html/universal_block_logs/block_log_open_sign
COPY block_log_single_sign /usr/share/nginx/html/universal_block_logs/block_log_single_sign
COPY block_log_multi_sign /usr/share/nginx/html/universal_block_logs/block_log_multi_sign
COPY block_log_maximum_sign /usr/share/nginx/html/universal_block_logs/block_log_maximum_sign
RUN sed -i "/index  index.html index.htm;/a \    autoindex on;" /etc/nginx/conf.d/default.conf
EXPOSE 80
EOF
  cat Dockerfile
  echo "Build docker image containing universal_block_logs"
  docker build -t $img .
  docker push $img
  echo "Created and push docker image with universal block logs: $img"
fi

echo "UNIVERSAL_BLOCK_LOG_LATEST_VERSION_IMAGE=$img" > $CI_PROJECT_DIR/universal_block_log_latest_version.env
echo "UNIVERSAL_BLOCK_LOG_LATEST_COMMIT_SHORT_SHA=$short_commit" > $CI_PROJECT_DIR/universal_block_log_latest_commit_short_sha.env
