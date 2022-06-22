#! /bin/bash

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
echo "$SCRIPTPATH"

LOG_FILE=run_hived_img.log
source "$SCRIPTPATH/common.sh"

log_exec_params "$@"

# Script reponsible for starting a docker container built for image specified at command line.

print_help () {
    echo "Usage: $0 <docker_img> [OPTION[=VALUE]]... [<hived_option>]..."
    echo
    echo "Allows to start docker container for a pointed hived docker image."
    echo "OPTIONS:"
    echo "  --webserver-http-endpoint=<endpoint>  Allows to map hived internal http endpoint to specified external one. As endpoint can be passed simple port number or ip-address:port."
    echo "  --webserver-ws-endpoint=<endpoint>    Allows to map hived internal WS endpoint to specified external one. As endpoint can be passed simple port number or ip-address:port."
    echo "  --p2p-endpoint=<endpoint>             Allows to map hived internal P2P endpoint to specified external one. As endpoint can be passed simple port number or ip-address:port."
    echo "  --data-dir=DIRECTORY_PATH             Allows to specify given directory as hived data directory. This directory should contain a config.ini file to be used by hived"
    echo "  --shared-file-dir=DIRECTORY_PATH      Allows to specify dedicated location for shared_memory_file.bin"
    echo "  --name=CONTAINER_NAME                 Allows to specify a dedicated name to the spawned container instance"
    echo "  --docker-option=OPTION                Allows to specify additional docker option, to be passed to underlying docker run spawn."
    echo "  --help                                Display this help screen and exit"
    echo
}

DOCKER_ARGS=()
HIVED_ARGS=()

CONTAINER_NAME=instance
IMAGE_NAME=

add_docker_arg() {
  local arg="$1"
#  echo "Processing docker argument: ${arg}"
  
  DOCKER_ARGS+=("$arg")
}

add_hived_arg() {
  local arg="$1"
#  echo "Processing hived argument: ${arg}"
  
  HIVED_ARGS+=("$arg")
}

while [ $# -gt 0 ]; do
  case "$1" in
    --webserver-http-endpoint=*)
        HTTP_ENDPOINT="${1#*=}"
        add_docker_arg "--publish=${HTTP_ENDPOINT}:8090"
        ;;
    --webserver-ws-endpoint=*)
        WS_ENDPOINT="${1#*=}"
        add_docker_arg "--publish=${WS_ENDPOINT}:8090"
        ;;
    --p2p-endpoint=*)
        P2_ENDPOINT="${1#*=}"
        add_docker_arg "--publish=${P2_ENDPOINT}:2001"
        ;;
    --data-dir=*)
        HIVED_DATADIR="${1#*=}"
        add_docker_arg "-v ${HIVED_DATADIR}:/home/hived/datadir/"
        ;;
    --shared-file-dir=*)
        HIVED_SHM_FILE_DIR="${1#*=}"
        # do not pass this arg to the hived directly - it has already passed path: /home/hived/shm_dir inside docker_entrypoint.sh
        add_docker_arg "-v ${HIVED_SHM_FILE_DIR}:/home/hived/shm_dir"
        ;;
     --name=*)
        CONTAINER_NAME="${1#*=}"
        echo "Container name is: $CONTAINER_NAME"
        ;;
    --docker-option=*)
        option="${o#*=}"
        add_docker_arg "$option"
        ;; 
    --help)
        print_help
        exit 0
        ;;
     -*)
        add_hived_arg "$1"
        ;;
     *)
        IMAGE_NAME="${1}"
        echo "Using image name: $IMAGE_NAME"
        ;;
    esac
    shift
done

# Collect remaining command line args to pass to the container to run
CMD_ARGS=("$@")

if [ -z "$IMAGE_NAME" ]; then
  echo "Error: Missing docker image name."
  echo "Usage: $0 <docker_img> [OPTION[=VALUE]]... [<hived_option>]..."
  echo
  exit 1
fi

CMD_ARGS+=("${HIVED_ARGS[@]}")

#echo "Using docker image: $IMAGE_NAME"
#echo "Additional hived args: ${CMD_ARGS[@]}"

docker container rm -f -v "$CONTAINER_NAME" 2>/dev/null || true
docker run --rm -itd -e UID=$(id -u) -e GID=$(id -g) --name "$CONTAINER_NAME" --stop-timeout=180 ${DOCKER_ARGS[@]} "${IMAGE_NAME}" "${CMD_ARGS[@]}"


