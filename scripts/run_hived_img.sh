#! /bin/bash

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 || exit 1; pwd -P )"
echo "$SCRIPTPATH"

export LOG_FILE=run_hived_img.log
# shellcheck source=common.sh
source "$SCRIPTPATH/common.sh"

log_exec_params "$@"

# This script starts a docker container for a hived image specified on the command line

print_help () {
cat <<EOF
  Usage: $0 <hived_docker_img> [OPTION[=VALUE]]... [<hived_option>]...

  Starts a docker container for a hived docker image.
  OPTIONS:
    --webserver-http-endpoint=<endpoint>  Maps hived internal http endpoint to the specified external one. Endpoint can be a port number or ip-address:port.
    --webserver-ws-endpoint=<endpoint>    Maps hived internal WS endpoint to the specified external one. Endpoint can be a port number or ip-address:port.
    --p2p-endpoint=<endpoint>             Map hived internal P2P endpoint to the specified external one. Endpoint can be a port number or ip-address:port.
    --data-dir=DIRECTORY_PATH             Specify path to a hived data directory on the host.
    --shared-file-dir=DIRECTORY_PATH      Specify directory path to a shared_memory_file.bin on the host.
    --name=CONTAINER_NAME                 Specify a dedicated name for the spawned container instance.
    --rm=true/false                       Automatically remove container once it's stopped (default: true)"
    --detach                              Starts the container instance in detached mode. Note: you can detach later using Ctrl+p+q key binding.
    --docker-option=OPTION                Specify a docker option to be passed to the underlying docker run command.
    --help,-h,-?                          Display this help screen and exit

EOF
}

DOCKER_ARGS=()
HIVED_ARGS=()

CONTAINER_NAME=instance
IMAGE_NAME=
HIVED_DATADIR=
HIVED_SHM_FILE_DIR=
AUTOREMOVE=true

HTTP_ENDPOINT="0.0.0.0:8091"
WS_ENDPOINT="0.0.0.0:8090"
P2_ENDPOINT="0.0.0.0:2001"

add_docker_arg() {
  local arg="$1"
  # echo "Processing docker argument: ${arg}"
  
  DOCKER_ARGS+=("$arg")
}

add_hived_arg() {
  local arg="$1"
  # echo "Processing hived argument: ${arg}"
  
  HIVED_ARGS+=("$arg")
}

while [ $# -gt 0 ]; do
  case "$1" in
   --webserver-http-endpoint=*)
        HTTP_ENDPOINT="${1#*=}"
        ;;
   --webserver-http-endpoint) 
        shift
        HTTP_ENDPOINT="${1}"
        ;;

    --webserver-ws-endpoint=*)
        WS_ENDPOINT="${1#*=}"
        ;;
    --webserver-ws-endpoint)
        shift
        WS_ENDPOINT="${1}"
        ;;

    --p2p-endpoint=*)
        P2_ENDPOINT="${1#*=}"
        ;;
    --p2p-endpoint)
        shift
        P2_ENDPOINT="${1}"
        ;;

    --data-dir=*)
        HIVED_DATADIR="${1#*=}"
        add_docker_arg "--volume"
        add_docker_arg "${HIVED_DATADIR}:/home/hived/datadir/"
        ;;
    --data-dir)
        shift
        HIVED_DATADIR="${1}"
        add_docker_arg "--volume"
        add_docker_arg "${HIVED_DATADIR}:/home/hived/datadir/"
        ;;

    --shared-file-dir=*)
        HIVED_SHM_FILE_DIR="${1#*=}"
        ;;
    --shared-file-dir)
        shift
        HIVED_SHM_FILE_DIR="${1}"
        ;;

     --name=*)
        CONTAINER_NAME="${1#*=}"
        echo "Container name is: $CONTAINER_NAME"
        ;;
     --name)
        shift
        CONTAINER_NAME="${1}"
        echo "Container name is: $CONTAINER_NAME"
        ;;

    --rm=*)
        AUTOREMOVE="${1#*=}"
        echo "Autoremove is set to: $AUTOREMOVE"
        ;;

    --detach)
      add_docker_arg "--detach"
      ;;

    --docker-option=*)
        # Since some of the Docker options are technicaly two separate args (eg --env SOME_ENV=value)
        # the proper way to pass them is to split them and pass the args one at a time
        options_string="${1#*=}"
        IFS=" " read -ra options <<< "$options_string"
        for option in "${options[@]}"; do
          add_docker_arg "$option"
        done
        ;;
    --docker-option)
        shift
        options_string="${1}"
        IFS=" " read -ra options <<< "$options_string"
        for option in "${options[@]}"; do
          add_docker_arg "$option"
        done
        ;;

    --help|-h|-?)
        print_help
        exit 0
        ;;
     -*)
        add_hived_arg "$1"
        ;;
     *)
        if [ -z "$IMAGE_NAME" ]; then
            IMAGE_NAME="${1}"
            echo "Using image name: $IMAGE_NAME"
        else
          add_hived_arg "${1}"
        fi
        ;;
    esac
    shift
done

# Collect remaining command line args to pass to the container to run
CMD_ARGS=("$@")

if [ -z "$IMAGE_NAME" ]; then
  echo "Error: Missing required docker image name."
  echo "Usage: $0 <hived_docker_img> [OPTION[=VALUE]]... [<hived_option>]..."
  echo
  exit 1
fi

CMD_ARGS+=("${HIVED_ARGS[@]}")

if [ -n "$HIVED_DATADIR" ]; then

  if [ -z "$HIVED_SHM_FILE_DIR" ]; then
    # if the datadir has been specified, but the shm directory mapping has not, store the shm file in the blockchain subdirectory of the datadir in order to save the shm file outside the container.
    HIVED_SHM_FILE_DIR="${HIVED_DATADIR}/blockchain"
  fi
fi

if [ -n "$HIVED_SHM_FILE_DIR" ]; then
  mkdir -p "${HIVED_SHM_FILE_DIR}"

  # do not pass this arg to the hived directly - it has already passed path: pointed by SHM_DIR variable inside docker_entrypoint.sh
  add_docker_arg "--env"
  add_docker_arg "SHM_DIR=/home/hived/shm_dir"
  add_docker_arg "--volume"
  add_docker_arg "${HIVED_SHM_FILE_DIR}:/home/hived/shm_dir"
fi

if [ "$AUTOREMOVE" == "true" ]; then
  add_docker_arg "--rm"
fi

# Similar to hived, the dockerized version should also immediately start listening on the specified ports 
add_docker_arg "--publish=${HTTP_ENDPOINT}:8091"
add_docker_arg "--publish=${WS_ENDPOINT}:8090"
add_docker_arg "--publish=${P2_ENDPOINT}:2001"

# echo "Using docker image: $IMAGE_NAME"
# echo "Additional hived args: ${CMD_ARGS[@]}"

# If command 'tput' exists
if command -v tput &> /dev/null
then
  add_docker_arg "--env"
  add_docker_arg "COLUMNS=$(tput cols)"
  add_docker_arg "--env"
  add_docker_arg "LINES=$(tput lines)"
fi

docker container rm -f -v "$CONTAINER_NAME" 2>/dev/null || true

# echo "DOCKER_ARGS: ${DOCKER_ARGS[*]}"

# Scritps should use long options for better readability
docker run --interactive --tty --env HIVED_UID="$(id --user)" --name "$CONTAINER_NAME" --stop-timeout=180 "${DOCKER_ARGS[@]}" "${IMAGE_NAME}" "${CMD_ARGS[@]}"
