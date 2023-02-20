#! /bin/bash
set -xeuo pipefail

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
SCRIPTSDIR="$SCRIPTPATH/.."
SRCROOTDIR="$SCRIPTSDIR/.."

source "$SCRIPTPATH/docker_image_utils.sh"


submodule_path=""
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


IMGNAME=base_instance
IMGNAME_INSTANCE=instance

while [ $# -gt 0 ]; do
  case "$1" in
    --network-type=*)
        NETWORK_TYPE="${1#*=}"
        echo "using NETWORK_TYPE $NETWORK_TYPE"

        case $NETWORK_TYPE in
          "testnet"*)
            IMGNAME=testnet-base_instance
            IMGNAME_INSTANCE=testnet-instance
            ;;
          "mirrornet"*)
            IMGNAME=mirrornet-base_instance
            IMGNAME_INSTANCE=mirrornet-instance
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

commit=$( $SCRIPTPATH/retrieve_last_commit.sh ${submodule_path} )
echo "commit with last source code changes is $commit"

img=$( build_image_name $IMGNAME $commit $REGISTRY )
img_path=$( build_image_registry_path $IMGNAME $commit $REGISTRY )
img_tag=$( build_image_registry_tag $IMGNAME $commit $REGISTRY )

img_instance=$( build_image_name $IMGNAME_INSTANCE $commit $REGISTRY )

echo $REGISTRY_PASSWORD | docker login -u $REGISTRY_USER $REGISTRY --password-stdin

image_exists=0

docker_image_exists $IMGNAME $commit $REGISTRY image_exists

if [ "$image_exists" -eq 1 ];
then
  echo "Image already exists..."
  "$SCRIPTPATH/export-binaries.sh" ${img} "${BINARY_CACHE_PATH}"
else
  # Here continue an image build.
  echo "${img} image is missing. Building it..."

  "$SCRIPTPATH/build_instance4commit.sh" $commit $REGISTRY --export-binaries="${BINARY_CACHE_PATH}" --network-type=$NETWORK_TYPE
  time docker push $img
  time docker push $img_instance
fi

echo "$DOTENV_VAR_NAME=$img" > docker_image_name.env
echo "${DOTENV_VAR_NAME}_REGISTRY_PATH=$img_path" >> docker_image_name.env
echo "${DOTENV_VAR_NAME}_REGISTRY_TAG=$img_tag" >> docker_image_name.env

cat docker_image_name.env
