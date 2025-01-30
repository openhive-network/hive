#! /bin/bash
set -xeuo pipefail

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"

# shellcheck source=./docker_image_utils.sh
source "$SCRIPTPATH/docker_image_utils.sh"


submodule_path=""
# the path to the registry for this project *without a trailing slash*
REGISTRY=""
DOTENV_VAR_NAME=""
REGISTRY_USER=""
REGISTRY_PASSWORD=""

NETWORK_TYPE=""
EXPORT_BINARIES=""


print_help () {
    echo "Usage: $0 <submodule_path> <registry> <dotenv_var_name> <registry_user> <registry_password> [OPTION[=VALUE]]..."
    echo
    echo "Allows to build docker image containing Hived installation"
    echo "OPTIONS:"
    echo "  --network-type=TYPE         Allows to specify type of blockchain network supported by built hived. Allowed values: mainnet, testnet, mirrornet"
    echo "  --export-binaries=PATH      Allows to specify a path where binaries shall be exported from built image."
    echo "  --help                      Display this help screen and exit"
    echo
}


IMGNAME=base
IMGNAME_INSTANCE=
IMGNAME_MINIMAL_INSTANCE=minimal
# For use in haf_api_node, we always tag the minimal instance with the top-level
# registry path for the project.  Normally, CI_REGISTRY_IMAGE for that project,
# so registry.gitlab.syncad.com/hive/haf for this project.
# Ideally, we'd rename registry.gitlab.syncad.com/hive/haf/minimal-instance to
# registry.gitlab.syncad.com/hive/haf.  I don't know if anything depends on
# the existence of an image named minimal-instance.  For that reason,
# I'm just adding an additional tag that points to the same minimal-instance
# image.  If it's determined to be safe, we should remove the tag for
# minimal-instance.
RETAG_MINIMAL_INSTANCE=1

while [ $# -gt 0 ]; do
  case "$1" in
    --network-type=*)
        NETWORK_TYPE="${1#*=}"
        echo "using NETWORK_TYPE $NETWORK_TYPE"

        case $NETWORK_TYPE in
          "testnet"*)
            IMGNAME=testnet-base
            IMGNAME_INSTANCE=testnet
            IMGNAME_MINIMAL_INSTANCE=testnet-minimal
            RETAG_MINIMAL_INSTANCE=0
            ;;
          "mirrornet"*)
            IMGNAME=mirrornet-base
            IMGNAME_INSTANCE=mirrornet
            IMGNAME_MINIMAL_INSTANCE=mirrornet-minimal
            RETAG_MINIMAL_INSTANCE=0
            ;;
          "mainnet"*)
            ;;
           *)
            echo "ERROR: '$NETWORK_TYPE' is not a valid network type"
            echo
            exit 3
        esac
        ;;
    --export-binaries=*)
        EXPORT_BINARIES="${1#*=}"
        echo "using EXPORT_BINARIES $EXPORT_BINARIES"
        ;;
    --help)
        print_help
        exit 0
        ;;
    *)
        if [ -z "$submodule_path" ];
        then
          submodule_path="${1}"
        elif [ -z "$REGISTRY" ];
        then
          REGISTRY="${1}"
        elif [ -z "$DOTENV_VAR_NAME" ];
        then
          DOTENV_VAR_NAME=${1}
        elif [ -z "$REGISTRY_USER" ];
        then
          REGISTRY_USER=${1}
        elif [ -z "$REGISTRY_PASSWORD" ];
        then
          REGISTRY_PASSWORD=${1}
        else
          echo "ERROR: '$1' is not a valid option/positional argument"
          echo
          print_help
          exit 2
        fi
        ;;
    esac
    shift
done

echo "Attempting to get commit for: $submodule_path"

CHANGES=(libraries/ programs/ scripts/ docker/ tests/unit/ Dockerfile cmake CMakeLists.txt)
commit=$("$SCRIPTPATH/retrieve_last_commit.sh" "${submodule_path}" "${CHANGES[@]}")
echo "commit with last source code changes is $commit"

pushd "${submodule_path}"
short_commit=$(git -c core.abbrev=8 rev-parse --short "$commit")
popd

img=$( build_image_name "$short_commit" "$REGISTRY" $IMGNAME )
img_path=$( build_image_registry_path "$short_commit" "$REGISTRY" $IMGNAME )
img_tag="$short_commit"

img_instance=$( build_image_name "$short_commit" "$REGISTRY" $IMGNAME_INSTANCE )
img_minimal_instance=$( build_image_name "$short_commit" "$REGISTRY" $IMGNAME_MINIMAL_INSTANCE )
echo "$REGISTRY_PASSWORD" | docker login -u "$REGISTRY_USER" "$REGISTRY" --password-stdin

image_exists=0

docker_image_exists "$short_commit" "$REGISTRY" $IMGNAME image_exists

if [ "$image_exists" -eq 1 ];
then
  echo "Image already exists..."
  "$SCRIPTPATH/export-data-from-docker-image.sh" "${img}" "${BINARY_CACHE_PATH}"
else
  # Here continue an image build.
  echo "${img} image is missing. Building it..."

  "$SCRIPTPATH/build_instance4commit.sh" "$commit" "$REGISTRY" --export-binaries="${BINARY_CACHE_PATH}" --network-type="$NETWORK_TYPE" --image-tag="$short_commit"
  time docker push "$img"
  time docker push "$img_instance"
  time docker push "$img_minimal_instance"
  if [ "$RETAG_MINIMAL_INSTANCE" -eq 1 ]; then
    echo "Retagging minimal-instance"
    docker tag "$img_minimal_instance" "${REGISTRY}:${short_commit}"
    time docker push "${REGISTRY}:${short_commit}"
  fi
fi

echo "${DOTENV_VAR_NAME}_IMAGE_NAME=$img" > docker_image_name.env
{
  echo "${DOTENV_VAR_NAME}_BASE_INSTANCE=$img"
  echo "${DOTENV_VAR_NAME}_INSTANCE=$img_instance"
  echo "${DOTENV_VAR_NAME}_MINIMAL_INSTANCE=$img_minimal_instance"
  echo "${DOTENV_VAR_NAME}_REGISTRY_PATH=$img_path"
  echo "${DOTENV_VAR_NAME}_REGISTRY_TAG=$img_tag"
  echo "${DOTENV_VAR_NAME}_COMMIT=$commit"
} >> docker_image_name.env

cat docker_image_name.env
