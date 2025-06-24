#!/bin/bash

set -e

# Paths
SCRIPT_DIR="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 || exit 1; pwd -P )"
source "$SCRIPT_DIR/docker_image_utils.sh"
SRC_DIR="$(realpath "${SCRIPT_DIR}/../..")"
BINARY_PATH="${BINARY_PATH:?}"
MIRRORNET_SOURCE_5M_DATA_DIR=${MIRRORNET_SOURCE_5M_DATA_DIR:?}
MAINNET_TRUNCATED_DIR=${MAINNET_TRUNCATED_DIR:?}
MIRRORNET_BLOCKCHAIN_DATA_DIR=${MIRRORNET_BLOCKCHAIN_DATA_DIR:?}
EXTENDED_MIRRORNET_BLOCKCHAIN_DATA_DIR=${EXTENDED_MIRRORNET_BLOCKCHAIN_DATA_DIR:?}
HIVED_PATH=${HIVED_PATH:-"${BINARY_PATH}/hived"}
CLI_WALLET_PATH=${CLI_WALLET_PATH:-"${BINARY_PATH}/cli_wallet"}
GET_DEV_KEY_PATH=${GET_DEV_KEY_PATH:-"${BINARY_PATH}/get_dev_key"}
COMPRESS_BLOCK_LOG_PATH=${COMPRESS_BLOCK_LOG_PATH:-"${BINARY_PATH}/compress_block_log"}
BLOCKCHAIN_CONVERTER_PATH=${BLOCKCHAIN_CONVERTER_PATH:-"${BINARY_PATH}/blockchain_converter"}
BLOCK_LOG_UTIL_PATH=${BLOCK_LOG_UTIL_PATH:-"${BINARY_PATH}/block_log_util"}

IMAGE_OUTPUT="--load"
for arg in "$@"; do
  if [ "$arg" == "--push" ]; then
    IMAGE_OUTPUT="--push"
    break
  fi
done

# The image tag is generated from the checksum of these source files.
IMAGE_SOURCE_FILES=(
  "scripts/ci-helpers/extended_block_log_creation.gitlab-ci.yml"
  "scripts/ci-helpers/prepare_extended_mirrornet_block_log.sh"
  "scripts/ci-helpers/prepare_extended_mirrornet_block_log_for_commit.sh"
  "tests/python/functional/util/block_logs_for_denser/generate_block_log_for_denser.py"
)
IMAGE_TAG=$(cat "${IMAGE_SOURCE_FILES[@]}" | sha256sum | tr -d '[:blank:] [=-=]')
IMG_NAME=extended-block-log

IMAGE_FULL_NAME=$( build_image_name "$IMAGE_TAG" "$REGISTRY" $IMG_NAME )

# Python settings
export PYPROJECT_DIR="${SRC_DIR}/tests/python/hive-local-tools"
export PYPROJECT_CONFIG_PATH="${SRC_DIR}/tests/python/hive-local-tools/pyproject.toml"
# export HIVE_BUILD_ROOT_PATH="${BINARY_PATH}"
export HIVED_PATH
export CLI_WALLET_PATH
export GET_DEV_KEY_PATH
export COMPRESS_BLOCK_LOG_PATH
export BLOCK_LOG_UTIL_PATH

# Blockchain parameters
MIRRORNET_CHAIN_ID=${MIRRORNET_CHAIN_ID:-"44"}
MIRRORNET_SKELETON_KEY=${MIRRORNET_SKELETON_KEY:-"5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n"}

# Other settings
NUMBER_OF_BLOCKS=${NUMBER_OF_BLOCKS:-"5000000"}
NUMBER_OF_PROCESSES=${NUMBER_OF_PROCESSES:-"8"}

function generate-env() {
    echo "EXTENDED_BLOCK_LOG_IMAGE=${IMAGE_FULL_NAME}" > "${SRC_DIR}/block_log.env"
}

IMAGE_EXIST=0
docker_image_exists "$IMAGE_FULL_NAME" IMAGE_EXIST
if [ "$IMAGE_EXIST" -eq 1 ];
then
    echo "Image $IMAGE_FULL_NAME already exists. Skipping build..."
    generate-env
    exit 0
fi

echo "${IMAGE_FULL_NAME} image is missing. Start extended mirrornet block logs..."

pushd "${SRC_DIR}"

mkdir -p "${MIRRORNET_SOURCE_5M_DATA_DIR}"
mkdir -p "${MAINNET_TRUNCATED_DIR}"
mkdir -p "${MIRRORNET_BLOCKCHAIN_DATA_DIR}"
mkdir -p "${EXTENDED_MIRRORNET_BLOCKCHAIN_DATA_DIR}"

if [[ ! -e "${MIRRORNET_SOURCE_5M_DATA_DIR}/block_log.artifacts" ]]
then
    echo "Generating blog_log.artifacts file..."
    time "${BLOCK_LOG_UTIL_PATH}" --generate-artifacts --block-log "${MIRRORNET_SOURCE_5M_DATA_DIR}/block_log"
fi

echo "Compressing block log to ${NUMBER_OF_BLOCKS} blocks with ${NUMBER_OF_PROCESSES} processes..."
time "${COMPRESS_BLOCK_LOG_PATH}" \
    -i "${MIRRORNET_SOURCE_5M_DATA_DIR}/block_log" \
    -o "${MAINNET_TRUNCATED_DIR}/block_log" \
    --decompress \
    -n "${NUMBER_OF_BLOCKS}" \
    --jobs "${NUMBER_OF_PROCESSES}"

echo "Converting block log to mirrornet format with ${NUMBER_OF_PROCESSES} processes..."
time "${BLOCKCHAIN_CONVERTER_PATH}" \
    --plugin block_log_conversion \
    -i "${MAINNET_TRUNCATED_DIR}/block_log" \
    -o "${MIRRORNET_BLOCKCHAIN_DATA_DIR}/block_log" \
    --chain-id "${MIRRORNET_CHAIN_ID}" \
    --private-key "${MIRRORNET_SKELETON_KEY}" \
    --use-same-key \
    --jobs "${NUMBER_OF_PROCESSES}"

echo "Installing Python dependencies..."
poetry -C "${PYPROJECT_DIR}" env use python3
# shellcheck source=/dev/null
source "$(poetry -C "${PYPROJECT_DIR}" env info --path)/bin/activate"
poetry -C "${PYPROJECT_DIR}" install
python3 -m pip install pytz

echo "Extending the block log"
time python3 "${SRC_DIR}/tests/python/functional/util/block_logs_for_denser/generate_block_log_for_denser.py" \
    --input-block-log-directory="${MIRRORNET_BLOCKCHAIN_DATA_DIR}" \
    --output-block-log-directory="${EXTENDED_MIRRORNET_BLOCKCHAIN_DATA_DIR}"

echo "Generating blog_log.artifacts file for extended block_log..."
time "${BLOCK_LOG_UTIL_PATH}" --generate-artifacts --block-log "${EXTENDED_MIRRORNET_BLOCKCHAIN_DATA_DIR}/block_log"

cat <<EOF > "${EXTENDED_MIRRORNET_BLOCKCHAIN_DATA_DIR}/Dockerfile"
FROM scratch
COPY *.json /blockchain/
COPY block_log* /blockchain/
EOF

time docker buildx build \
  --progress="plain" \
  --tag "$IMAGE_FULL_NAME" \
  "$IMAGE_OUTPUT" \
  --file "${EXTENDED_MIRRORNET_BLOCKCHAIN_DATA_DIR}/Dockerfile" \
  "${EXTENDED_MIRRORNET_BLOCKCHAIN_DATA_DIR}"

if [ "$IMAGE_OUTPUT" = "--push" ]; then
  echo "Image pushed: $IMAGE_FULL_NAME"
else
  echo "Image built locally: $IMAGE_FULL_NAME"
fi

generate-env

popd