#! /bin/bash

set -euo pipefail

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
SCRIPTSDIR="$SCRIPTPATH/.."
SRCROOTDIR="$SCRIPTSDIR/.."

IMGNAME=data

source "$SCRIPTPATH/docker_image_utils.sh"

submodule_path=${1:?"Missing arg 1 for submodule path variable"}
shift
REGISTRY=${1:?"Missing arg 2 for REGISTRY variable"}
shift
DOTENV_VAR_NAME=${1:?"Missing name of dot-env variable"}
shift
REGISTRY_USER=${1:?"Missing arg 4 for REGISTRY_USER variable"}
shift
REGISTRY_PASSWORD=${1:?"Missing arg 5 for REGISTRY_PASSWORD variable"}
shift
BINARY_CACHE_PATH=${1:?"Missing arg 6 specific to binary cache path"}
shift

retrieve_submodule_commit () {
  local p="${1}"
  pushd "$p" >/dev/null 2>&1
  local commit=$( git rev-parse HEAD )

  popd >/dev/null 2>&1

  echo "$commit"
}

echo "Attempting to get commit for: $submodule_path"

commit=$( retrieve_submodule_commit ${submodule_path} )

img=$( build_image_name $IMGNAME $commit $REGISTRY )
img_path=$( build_image_registry_path $IMGNAME $commit $REGISTRY )
img_tag=$( build_image_registry_tag $IMGNAME $commit $REGISTRY )


echo $REGISTRY_PASSWORD | docker login -u $REGISTRY_USER $REGISTRY --password-stdin

image_exists=0

docker_image_exists $IMGNAME $commit $REGISTRY image_exists

if [ "$image_exists" -eq 1 ];
then
  # Todo otherwise image must be pulled, what can be timeconsuming
  echo "Image already exists - binaries shall be available in cache"

  echo Attempting to export built binaries into directory: "${BINARY_CACHE_PATH}"
  docker build -o "${BINARY_CACHE_PATH}" - << EOF
    FROM scratch
    COPY --from=${img} /home/hived/bin/ /
EOF

else
  # Here continue an image build.
  echo "${img} image is missing. Building it..."
  "$SCRIPTPATH/build_data4commit.sh" $commit $REGISTRY --export-binaries="${BINARY_CACHE_PATH}"
  docker push $img
fi

echo "$DOTENV_VAR_NAME=$img" > docker_image_name.env
echo "${DOTENV_VAR_NAME}_REGISTRY_PATH=$img_path" >> docker_image_name.env
echo "${DOTENV_VAR_NAME}_REGISTRY_TAG=$img_tag" >> docker_image_name.env

cat docker_image_name.env
