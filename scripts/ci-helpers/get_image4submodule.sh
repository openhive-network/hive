#! /bin/bash
# shellcheck source-path=SCRIPTDIR

set -euo pipefail

SCRIPTPATH=$(realpath "$0")
SCRIPTPATH=$(dirname "$SCRIPTPATH")

IMGNAME=data

source "$SCRIPTPATH/docker_image_utils.sh"

SUBMODULE_PATH=${1:?"Missing argument #1: submodule path"}
shift
REGISTRY=${1:?"Missing argument #2: image registry"}
shift
DOTENV_VAR_NAME=${1:?"Missing argument #3: dot-env name"}
shift
REGISTRY_USER=${1:?"Missing argument #4: registry user"}
shift
REGISTRY_PASSWORD=${1:?"Missing argument #5: registry password"}
shift
BINARY_CACHE_PATH=${1:?"Missing argument #6: binary cache path"}
shift
BRANCH_NAME=${1:?"Missing argument #7: branch to get buildkit cache for"}
shift

retrieve_submodule_commit () {
  local -r p="${1}"
  pushd "$p" >/dev/null 2>&1
  local -r commit=$( git rev-parse HEAD )

  popd >/dev/null 2>&1

  echo "$commit"
}

echo "Attempting to get commit for: $SUBMODULE_PATH"

commit=$( retrieve_submodule_commit "${SUBMODULE_PATH}" )

img=$( build_image_name $IMGNAME "$commit" "$REGISTRY" )
img_path=$( build_image_registry_path $IMGNAME "$commit" "$REGISTRY" )
img_tag=$( build_image_registry_tag $IMGNAME "$commit" "$REGISTRY" )

echo "$REGISTRY_PASSWORD" | docker login -u "$REGISTRY_USER" "$REGISTRY" --password-stdin

image_exists=0

docker_image_exists $IMGNAME "$commit" "$REGISTRY" image_exists

if [ "$image_exists" -eq 1 ];
then
  echo "Image already exists..."
  "$SCRIPTPATH/export-binaries.sh" "${img}" "${BINARY_CACHE_PATH}"
else
  # Here continue an image build.
  echo "${img} image is missing. Building it..."
  "$SCRIPTPATH/build_data4commit.sh" "$commit" "$REGISTRY" "$BRANCH_NAME" --export-binaries="${BINARY_CACHE_PATH}"
  time docker push "$img"
fi

echo "$DOTENV_VAR_NAME=$img" > docker_image_name.env
echo "${DOTENV_VAR_NAME}_REGISTRY_PATH=$img_path" >> docker_image_name.env
echo "${DOTENV_VAR_NAME}_REGISTRY_TAG=$img_tag" >> docker_image_name.env

cat docker_image_name.env
