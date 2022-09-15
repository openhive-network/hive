#! /bin/bash

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
SCRIPTSDIR="$SCRIPTPATH/.."


set -euo pipefail

LOG_FILE=build_and_setup_exchange_instance.log

# Because this script should work as standalone script being just downloaded from gitlab repo, and next use internal
# scripts from a cloned repo, logging code is duplicated.

exec > >(tee "${LOG_FILE}") 2>&1

log_exec_params() {
  echo
  echo -n "$0 parameters: "
  for arg in "$@"; do echo -n "$arg "; done
  echo
}

log_exec_params "$@"

print_help () {
    echo "Usage: $0 <image_tag> [OPTION[=VALUE]]... [<hived_option>]..."
    echo
    echo "Setup script for a Hived instance dedicated to use of exchange purposes."
    echo "Allows to (optionally build if not present) image containing preconfigured hived instance and next start a container holding it".
    echo "If you need to rebuild image, just remove the image using docker tools"
    echo
    echo "OPTIONS:"
    echo "  --webserver-http-endpoint=<endpoint>  Allows to map hived internal http endpoint to specified external one. As endpoint can be passed simple port number or ip-address:port."
    echo "  --webserver-ws-endpoint=<endpoint>    Allows to map hived internal WS endpoint to specified external one. As endpoint can be passed simple port number or ip-address:port."
    echo "  --p2p-endpoint=<endpoint>             Allows to map hived internal P2P endpoint to specified external one. As endpoint can be passed simple port number or ip-address:port."
    echo "  --data-dir=DIRECTORY_PATH             Obligatory. Allows to specify given directory as hived data directory. If this directory does not contain a config.ini file, default one will be created."
    echo "  --shared-file-dir=DIRECTORY_PATH      Allows to specify dedicated location for shared_memory_file.bin"
    echo
    echo "  --branch=branch                       Optionally specify a branch of Hived to checkout and build. Defaults to master branch."
    echo "  --use-source-dir=PATH                 Allows to specify explicit Hived source directory instead of performing git clone/checkout."
    echo
    echo "  --download-block-log                  Optional, allows to download block_log file(s) if they are missing using default url: \`https://gtg.openhive.network/get/blockchain/compressed/\`"
    echo "  --block-log=base-url=url              Optional, allows to specify custom source url and download Hive blockchain block_log file(s). File(s) will be downloaded into \`blockchain\` subdirectory located inside specified node data directory (by --data-dir option). If omitted, hived will synchronize using own P2P protocol."
    echo "  --name=CONTAINER_NAME                 Allows to specify a dedicated name to the spawned container instance"
    echo "  --docker-option=OPTION                Allows to specify additional docker option, to be passed to underlying docker run spawn."
    echo
    echo "  --option-file=PATH                    Allows to specify options through file"
    echo "  --help                                Display this help screen and exit"
    echo
}

REGISTRY="registry.gitlab.syncad.com/hive/hive/"
HIVED_IMAGE_NAME="instance"
HIVED_IMAGE_TAG=""

HIVED_SOURCE_DIR=""
HIVED_DATADIR=""
HIVED_BRANCH=master
HIVED_REPO_URL="https://gitlab.syncad.com/hive/hive.git"

BLOCK_LOG_BASE_URL="https://gtg.openhive.network/get/blockchain/compressed/"
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
  for o in ${READ_OPTIONS[@]}; do
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
  local image_name=${2}
  local block_log_base_url=${3}

  mkdir --mode=777 -p "${data_dir}/blockchain"

  if [ "${DOWNLOAD_BLOCK_LOG}" -eq 1 ];
  then
    if [ -f "${data_dir}/blockchain/block_log" ];
    then
      echo "block_log file exists at location: '${data_dir}/blockchain/' - skipping download."
    else
      echo "Downloading a block_log file to the location: '${data_dir}/blockchain/block_log'."
      wget -O "${data_dir}/blockchain/block_log" ${block_log_base_url}/block_log
    fi

    if [ -f "${data_dir}/blockchain/block_log.artifacts" ];
    then
      echo "block_log.artifacts file exists at location: '${data_dir}/blockchain/' - skipping download."
    else
      echo "Downloading a block_log.artifacts file to the location: '${data_dir}/blockchain/block_log.artifacts'."
      wget -O "${data_dir}/blockchain/block_log.artifacts" ${block_log_base_url}/block_log.artifacts
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

TST_IMGTAG=${HIVED_IMAGE_TAG:?"Missing arg #1 to specify image tag"}

TST_DATADIR=${HIVED_DATADIR:?"Missing --data-dir option to specify host directory where hived node data will be stored"}


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

prepare_data_directory "${HIVED_DATADIR}" ${img_name} ${BLOCK_LOG_BASE_URL}

"${HIVED_SOURCE_DIR}/scripts/run_hived_img.sh" ${img_name} ${IMPLICIT_SHM_DIR} "${RUN_HIVED_IMG_ARGS[@]}"

