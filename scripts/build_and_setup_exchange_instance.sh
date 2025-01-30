#! /bin/bash

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
SCRIPTSDIR="$SCRIPTPATH/.."


set -euo pipefail

LOG_FILE=build_and_setup_exchange_instance.log

# This script should work as a standalone script directly downloaded from the gitlab repo, and next use internal
# scripts from a cloned repo, so the logging code is duplicated.

exec > >(tee "${LOG_FILE}") 2>&1

log_exec_params() {
  echo
  echo -n "$0 parameters: "
  for arg in "$@"; do echo -n "$arg "; done
  echo
}

log_exec_params "$@"

print_help () {
    echo "Usage: $0 <hived_image_tag> [OPTION[=VALUE]]... [<hived_option>]..."
    echo
    echo "Builds a hived docker image (if not already built) preconfigured for use by a cryptocurrency exchange and starts a container for it."
    echo "If you need to rebuild the image, first manually remove the old image using docker tools."
    echo
    echo "OPTIONS:"
    echo "  --webserver-http-endpoint=<endpoint>  Map the hived internal http endpoint to the specified external one. Endpoints are specified either by port number or by ip-address:port."
    echo "  --webserver-ws-endpoint=<endpoint>    Map the hived internal WS endpoint to the specified external one. Endpoints are specified either by port number or by ip-address:port."
    echo "  --p2p-endpoint=<endpoint>             Map the hived internal P2P endpoint to the specified external one. Endpoints are specified either by port number or by ip-address:port."
    echo "  --data-dir=DIRECTORY_PATH             Obligatory. Specify path to the hived data directory. If this directory does not contain a config.ini file, a default one will be created."
    echo "  --shared-file-dir=DIRECTORY_PATH      Specify path for storing the shared_memory_file.bin file"
    echo
    echo "  --branch=branch                       Specify a branch of hive repo to checkout and build (defaults to master)."
    echo "  --use-source-dir=PATH                 Specify an existing hived source directory instead of performing a git clone/checkout."
    echo
    echo "  --download-block-log                  Download block_log file(s) if they are missing using default url: \`https://gtg.openhive.network/get/blockchain/\`"
    echo "  --block-log=base-url=url              Specify a custom url to download a Hive blockchain block_log file. It will be downloaded into the \`blockchain\` subdirectory located inside the data directory (by --data-dir option). If this option is omitted, hived will sync blocks directly from other nodes in the P2P network."
    echo "  --name=CONTAINER_NAME                 Specify a dedicated name for the hived container being spawned."
    echo "  --docker-option=OPTION                Specify a docker option to pass to the underlying docker run command."
    echo
    echo "  --option-file=PATH                    Specify these options with a text file."
    echo "  --help                                Display this help screen and exit."
    echo
}

REGISTRY="registry.gitlab.syncad.com/hive/hive"
HIVED_IMAGE_NAME=""
HIVED_IMAGE_TAG=""

HIVED_SOURCE_DIR=""
HIVED_DATADIR=""
HIVED_BRANCH=master
HIVED_REPO_URL="https://gitlab.syncad.com/hive/hive.git"

BLOCK_LOG_BASE_URL="https://gtg.openhive.network/get/blockchain/"
DOWNLOAD_BLOCK_LOG=0

RUN_HIVED_IMG_ARGS=()

IMPLICIT_SHM_DIR=""

add_run_hived_img_arg() {
  local arg="$1"
  RUN_HIVED_IMG_ARGS+=("$arg")
  echo "Adding new hived spawn arg: $arg"
}

process_option() {
  o="$1"
  case "$o" in
    --branch=*)
        HIVED_BRANCH="${o#*=}"
        ;;
    --use-source-dir=*)
        HIVED_SOURCE_DIR="${o#*=}"
        ;;
    --webserver-http-endpoint=*)
        add_run_hived_img_arg "${o}"
        ;;
    --webserver-ws-endpoint=*)
        add_run_hived_img_arg "${o}"
        ;;
    --p2p-endpoint=*)
        add_run_hived_img_arg "${o}"
        ;;
    --data-dir=*)
        HIVED_DATADIR="${1#*=}"
        IMPLICIT_SHM_DIR=--shared-file-dir="${HIVED_DATADIR}/blockchain"
        add_run_hived_img_arg "${o}"
        ;;
    --shared-file-dir=*)
        add_run_hived_img_arg "${o}"
        IMPLICIT_SHM_DIR=""
        ;;
    --download-block-log)
        DOWNLOAD_BLOCK_LOG=1
        ;;
    --block-log-base-url=*)
        BLOCK_LOG_BASE_URL="${1#*=}"
        DOWNLOAD_BLOCK_LOG=1
        ;;
     --name=*)
        add_run_hived_img_arg "${o}"
        ;;
    --docker-option=*)
        add_run_hived_img_arg "${o}"
        ;; 
    --help)
        print_help
        exit 0
        ;;
    -*)
        add_run_hived_img_arg "${o}"
        ;;
    *)
        if [ -z "${HIVED_IMAGE_TAG}" ];
        then
          HIVED_IMAGE_TAG="${1}"
        else
          echo "ERROR: '$1' is not a valid argument"
          echo
          print_help
          exit 2
        fi
        ;;
  esac
}

process_option_file() {
  local option_file="$1"
  local READ_OPTIONS=()
  IFS=

  mapfile -t <"$option_file" READ_OPTIONS
#  echo "Read options: ${READ_OPTIONS[@]}"
  for o in "${READ_OPTIONS[@]}"; do
    [[ $o =~ ^#.* ]] && continue
#    echo "Processing a file option: $o"
    process_option "$o"
  done
}

do_clone() {
  local branch=$1
  local src_dir="$2"
  echo "Cloning branch: $branch from $HIVED_REPO_URL ..."
  git clone --recurse-submodules --shallow-submodules --single-branch --depth=1 --branch "$branch" -- "$HIVED_REPO_URL" "$src_dir"
}

prepare_data_directory() {
  local data_dir=${1}
  local _image_name=${2}
  local block_log_base_url=${3}

  # --mode only applies to the blockchain directory when combined with -p.
  # Let's make that explicit to avoid confusion.
  mkdir -p "${data_dir}"
  mkdir --mode=777 "${data_dir}/blockchain"

  if [ "${DOWNLOAD_BLOCK_LOG}" -eq 1 ];
  then
    if [ -f "${data_dir}/blockchain/block_log" ];
    then
      echo "block_log file exists at location: '${data_dir}/blockchain/' - skipping download."
    else
      echo "Downloading a block_log file to the location: '${data_dir}/blockchain/block_log'."
      wget -O "${data_dir}/blockchain/block_log" "${block_log_base_url}/block_log"
    fi

    if [ -f "${data_dir}/blockchain/block_log.artifacts" ];
    then
      echo "block_log.artifacts file exists at location: '${data_dir}/blockchain/' - skipping download."
    else
      echo "Downloading a block_log.artifacts file to the location: '${data_dir}/blockchain/block_log.artifacts'."
      wget -O "${data_dir}/blockchain/block_log.artifacts" "${block_log_base_url}/block_log.artifacts"
    fi
  fi
  
}

while [ $# -gt 0 ]; do
  case "$1" in
    --option-file=*)
      filename="${1#*=}"
      process_option_file "$filename"
      ;;
    *)
      process_option "$1"
      ;;
  esac
  shift
done

_TST_IMGTAG=${HIVED_IMAGE_TAG:?"Missing arg #1 to specify image tag"}

_TST_DATADIR=${HIVED_DATADIR:?"Missing --data-dir option to specify host directory where hived node data will be stored"}


if [ -z "${HIVED_SOURCE_DIR}" ];
then
  # We need to clone sources.
  HIVED_SOURCE_DIR="hive-${HIVED_BRANCH}"
  
  rm -rf "./${HIVED_SOURCE_DIR}"
  do_clone "${HIVED_BRANCH}" "${HIVED_SOURCE_DIR}"
else
  HIVED_SOURCE_DIR=$( realpath -e "${HIVED_SOURCE_DIR}" )
  echo "Using pointed source directory: ${HIVED_SOURCE_DIR}"
fi

echo "Loading exchange-instance default option list from file: ${HIVED_SOURCE_DIR}/scripts/exchange_instance.conf"
process_option_file "${HIVED_SOURCE_DIR}/scripts/exchange_instance.conf"

# shellcheck source=ci-helpers/docker_image_utils.sh
source "${HIVED_SOURCE_DIR}/scripts/ci-helpers/docker_image_utils.sh"

image_exists=0

docker_image_exists "${HIVED_IMAGE_NAME}" "${HIVED_IMAGE_TAG}" "${REGISTRY}" image_exists 0

img_name=$( build_image_name "${HIVED_IMAGE_NAME}" "${HIVED_IMAGE_TAG}" "${REGISTRY}" )

if [ "$image_exists" -eq 1 ];
then
  echo "${img_name} image already exists... Skipping build."
else
  "${HIVED_SOURCE_DIR}/scripts/ci-helpers/build_instance.sh" "${HIVED_IMAGE_TAG}" "${HIVED_SOURCE_DIR}" "${REGISTRY}"
fi

prepare_data_directory "${HIVED_DATADIR}" "${img_name}" "${BLOCK_LOG_BASE_URL}"

"${HIVED_SOURCE_DIR}/scripts/run_hived_img.sh" "${img_name}" --docker-option="-p 8093:8093" "${IMPLICIT_SHM_DIR}" "${RUN_HIVED_IMG_ARGS[@]}"

