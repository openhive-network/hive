#!/bin/bash
#
# get_image4submodule.sh - Build or fetch a Docker image for the current source state
#
# This script finds the last commit that changed source files, checks if an image
# already exists for that commit, and either exports binaries from the existing
# image or builds a new one.
#
# Uses generic scripts from common-ci-configuration for commit lookup and registry checks.
#
set -euo pipefail

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"

# Fetch common-ci-configuration scripts
CI_SCRIPTS_DIR="${CI_SCRIPTS_DIR:-/tmp/common-ci-scripts}"
CI_SCRIPTS_REF="${CI_SCRIPTS_REF:-develop}"
CI_SCRIPTS_URL="https://gitlab.syncad.com/hive/common-ci-configuration/-/raw/${CI_SCRIPTS_REF}/scripts/bash"

if [[ ! -f "$CI_SCRIPTS_DIR/find-last-source-commit.sh" ]]; then
    echo "Fetching common-ci-configuration scripts..."
    mkdir -p "$CI_SCRIPTS_DIR"
    for script in find-last-source-commit.sh get-cached-image.sh; do
        curl -sf -o "$CI_SCRIPTS_DIR/$script" "$CI_SCRIPTS_URL/$script" || {
            echo "Error: Failed to fetch $script from common-ci-configuration"
            exit 1
        }
        chmod +x "$CI_SCRIPTS_DIR/$script"
    done
fi

# shellcheck source=/dev/null
source "$SCRIPTPATH/docker_image_utils.sh"

submodule_path=""
REGISTRY=""
DOTENV_VAR_NAME=""
REGISTRY_USER=""
REGISTRY_PASSWORD=""
NETWORK_TYPE=""

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

IMGNAME_INSTANCE=

while [ $# -gt 0 ]; do
  case "$1" in
    --network-type=*)
        NETWORK_TYPE="${1#*=}"
        echo "using NETWORK_TYPE $NETWORK_TYPE"

        case $NETWORK_TYPE in
          "testnet"*)
            IMGNAME_INSTANCE=testnet
            ;;
          "mirrornet"*)
            IMGNAME_INSTANCE=mirrornet
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
        # Ignored - binaries exported via BINARY_CACHE_PATH env var
        ;;
    --help)
        print_help
        exit 0
        ;;
    *)
        if [ -z "$submodule_path" ]; then
          submodule_path="${1}"
        elif [ -z "$REGISTRY" ]; then
          REGISTRY="${1}"
        elif [ -z "$DOTENV_VAR_NAME" ]; then
          DOTENV_VAR_NAME=${1}
        elif [ -z "$REGISTRY_USER" ]; then
          REGISTRY_USER=${1}
        elif [ -z "$REGISTRY_PASSWORD" ]; then
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

# Source patterns from single source of truth (also used by skip_rules.yml and downstream repos like clive)
IFS=',' read -ra SOURCE_PATTERNS <<< "$("$SCRIPTPATH/source-patterns.sh")"

# Find the last commit that changed source files
commit=$("$CI_SCRIPTS_DIR/find-last-source-commit.sh" --dir="$submodule_path" --full --quiet "${SOURCE_PATTERNS[@]}")
short_commit=$("$CI_SCRIPTS_DIR/find-last-source-commit.sh" --dir="$submodule_path" --quiet "${SOURCE_PATTERNS[@]}")

echo "commit with last source code changes is $commit (short: $short_commit)"

# Build image name
img_tag="$short_commit"
if [[ -n "$IMGNAME_INSTANCE" ]]; then
    img_path="${REGISTRY}/${IMGNAME_INSTANCE}"
    img_instance="${REGISTRY}/${IMGNAME_INSTANCE}:${img_tag}"
else
    img_path="${REGISTRY}"
    img_instance="${REGISTRY}:${img_tag}"
fi

# Login to registry
echo "$REGISTRY_PASSWORD" | docker login -u "$REGISTRY_USER" "$REGISTRY" --password-stdin

# Check if image exists
CACHE_ENV=$(mktemp)
if [[ -n "$IMGNAME_INSTANCE" ]]; then
    "$CI_SCRIPTS_DIR/get-cached-image.sh" --commit="$short_commit" --registry="$REGISTRY" --image="$IMGNAME_INSTANCE" --output="$CACHE_ENV" --quiet || true
else
    "$CI_SCRIPTS_DIR/get-cached-image.sh" --commit="$short_commit" --registry="$REGISTRY" --output="$CACHE_ENV" --quiet || true
fi
# shellcheck source=/dev/null
source "$CACHE_ENV"
rm -f "$CACHE_ENV"

if [[ "${CACHE_HIT:-false}" == "true" ]]; then
    echo "Image $img_instance already exists..."
    "$SCRIPTPATH/export-data-from-docker-image.sh" "${img_instance}" "${BINARY_CACHE_PATH}"
else
    # Build new image from current checkout (source files are identical to $commit)
    # This avoids an extra clone since GitLab already checked out the code
    echo "${img_instance} image is missing. Building from current checkout..."
    "$SCRIPTPATH/build_instance.sh" "$short_commit" "$submodule_path" "$REGISTRY" \
        --export-binaries="${BINARY_CACHE_PATH}" --network-type="$NETWORK_TYPE"
    time docker push "$img_instance"
fi

# Write output environment file
echo "${DOTENV_VAR_NAME}_IMAGE_NAME=$img_instance" > docker_image_name.env
{
    echo "${DOTENV_VAR_NAME}_INSTANCE=$img_instance"
    echo "${DOTENV_VAR_NAME}_REGISTRY_PATH=$img_path"
    echo "${DOTENV_VAR_NAME}_REGISTRY_TAG=$img_tag"
    echo "${DOTENV_VAR_NAME}_COMMIT=$commit"
} >> docker_image_name.env

cat docker_image_name.env
