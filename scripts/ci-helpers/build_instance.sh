#! /bin/bash

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
SCRIPTSDIR="$SCRIPTPATH/.."

LOG_FILE=build_instance.log
source "$SCRIPTSDIR/common.sh"

BUILD_IMAGE_TAG=""
REGISTRY=""
SRCROOTDIR=""

IMAGE_TAG_PREFIX=""

BLOCK_LOG_SUFFIX=""

BUILD_HIVE_TESTNET=OFF
HIVE_CONVERTER_BUILD=OFF


print_help () {
    echo "Usage: $0 <image_tag> <src_dir> <registry_url> [<block_log_img_suffix>] [OPTION[=VALUE]]..."
    echo
    echo "Allows to build docker image containing Hived installation"
    echo "OPTIONS:"
    echo "  --network-type=TYPE       Allows to specify type of blockchain network supported by built hived. Allowed values: mainnet, testnet, mirrornet"
    echo "  --export-binaries=PATH    Allows to specify a path where binaries shall be exported from built image."
    echo "  --help                    Display this help screen and exit"
    echo
}

EXPORT_PATH=""

while [ $# -gt 0 ]; do
  case "$1" in
    --network-type=*)
        type="${1#*=}"

        case $type in
          "testnet"*)
            BUILD_HIVE_TESTNET=ON
            IMAGE_TAG_PREFIX=testnet-
            ;;
          "mirrornet"*)
            BUILD_HIVE_TESTNET=OFF
            HIVE_CONVERTER_BUILD=ON
            IMAGE_TAG_PREFIX=mirror-
            ;;
          "mainnet"*)
            BUILD_HIVE_TESTNET=OFF
            HIVE_CONVERTER_BUILD=OFF
            IMAGE_TAG_PREFIX=
            ;;
           *)
            echo "ERROR: '$type' is not a valid network type"
            echo
            exit 3
        esac
        ;;
    --export-binaries=*)
        arg="${1#*=}"
        EXPORT_PATH="$arg"
        ;;
    --help)
        print_help
        exit 0
        ;;
    *)
        if [ -z "$BUILD_IMAGE_TAG" ];
        then
          BUILD_IMAGE_TAG="${1}"
        elif [ -z "$SRCROOTDIR" ];
        then
          SRCROOTDIR="${1}"
        elif [ -z "$REGISTRY" ];
        then
          REGISTRY=${1}
        elif [ -z "$BLOCK_LOG_SUFFIX" ];
        then
          BLOCK_LOG_SUFFIX="${1}"
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

TST_IMGTAG=${BUILD_IMAGE_TAG:?"Missing arg #1 to specify built image tag"}
TST_SRCDIR=${SRCROOTDIR:?"Missing arg #2 to specify source directory"}
TST_REGISTRY=${REGISTRY:?"Missing arg #3 to specify target container registry"}

# Supplement a registry path by trailing slash (if needed)
[[ "${REGISTRY}" != */ ]] && REGISTRY="${REGISTRY}/"

echo "Moving into source root directory: ${SRCROOTDIR}"

pushd "$SRCROOTDIR"
pwd

export DOCKER_BUILDKIT=1

docker build --target=base_instance \
  --build-arg BLOCK_LOG_SUFFIX="${BLOCK_LOG_SUFFIX}" \
  --build-arg CI_REGISTRY_IMAGE=$REGISTRY \
  --build-arg BUILD_HIVE_TESTNET=$BUILD_HIVE_TESTNET \
  --build-arg HIVE_CONVERTER_BUILD=$HIVE_CONVERTER_BUILD \
  --build-arg BUILD_IMAGE_TAG=$BUILD_IMAGE_TAG -t ${REGISTRY}base_instance${BLOCK_LOG_SUFFIX}:base_instance-${BUILD_IMAGE_TAG} -f Dockerfile .

# Build the image containing only binaries and be ready to start running hived instance, operating on mounted volummes pointing instance datadir and shm_dir
docker build --target=instance \
  --build-arg BLOCK_LOG_SUFFIX="${BLOCK_LOG_SUFFIX}" \
  --build-arg CI_REGISTRY_IMAGE=$REGISTRY \
  --build-arg BUILD_HIVE_TESTNET=$BUILD_HIVE_TESTNET \
  --build-arg HIVE_CONVERTER_BUILD=$HIVE_CONVERTER_BUILD \
  --build-arg BUILD_IMAGE_TAG=$BUILD_IMAGE_TAG -t ${REGISTRY}${IMAGE_TAG_PREFIX}instance${BLOCK_LOG_SUFFIX}:instance-${BUILD_IMAGE_TAG} -f Dockerfile .


popd

if [ ! -z "${EXPORT_PATH}" ];
then
  echo Attempting to export built binaries into directory: "${EXPORT_PATH}"
  docker build -o "${EXPORT_PATH}" - << EOF
    FROM scratch
    COPY --from=${REGISTRY}${IMAGE_TAG_PREFIX}instance${BLOCK_LOG_SUFFIX}:instance-${BUILD_IMAGE_TAG} /home/hived/bin/ /
EOF

fi

