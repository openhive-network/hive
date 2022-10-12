#! /bin/bash

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
echo "$SCRIPTPATH"

LOG_FILE=run_cli_wallet_img.log
source "$SCRIPTPATH/common.sh"

log_exec_params "$@"

print_help () {
    echo "Usage: $0 <docker_container_name> [OPTION[=VALUE]]... [<cli_wallet_option>]..."
    echo
    echo "Allows to start a cli_wallet placed inside started Hive docker container."
    echo "OPTIONS:"
    echo "  --rpc-http-endpoint=<endpoint>  Allows to set cli_wallet http endpoint to specified one. This should match to the port specified as cli_wallet port mapping at hived container start (defaults to 8093)"
    echo "  --rpc-http-allowip=ip_addr      Allows to specify the ip allowed to establish connections to the cli_wallet API HTTP server"
    echo "  --help                          Display this help screen and exit"
    echo
}

CLI_WALLET_ARGS=()

CONTAINER_NAME=instance

add_cli_wallet_arg() {
  local arg="$1"
#  echo "Processing cli_wallet argument: ${arg}"
  
  CLI_WALLET_ARGS+=("$arg")
}

while [ $# -gt 0 ]; do
  case "$1" in
    --rpc-http-endpoint=*)
        arg="${1#*=}"
        add_cli_wallet_arg "--daemon"
        add_cli_wallet_arg "--rpc-http-endpoint=${arg}"
        ;;
    --rpc-http-allowip=*)
        arg="${1#*=}"
        add_cli_wallet_arg "--rpc-http-allowip=${arg}"
        ;;
    -d*)
        ;; # ignore to avoid duplicated option error
    --daemon*)
        ;; # ignore to avoid duplicated option error

    --help)
        print_help
        exit 0
        ;;
     -*)
        add_cli_wallet_arg "$1"
        ;;
     *)
        CONTAINER_NAME="${1}"
        echo "Using container instance name: $CONTAINER_NAME"
        ;;
    esac
    shift
done

docker container exec -it ${CONTAINER_NAME} /home/hived/bin/cli_wallet --wallet-file=/home/hived/datadir/wallet.json "${CLI_WALLET_ARGS[@]}"
