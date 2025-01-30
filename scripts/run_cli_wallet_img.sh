#! /bin/bash

set -e

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
echo "$SCRIPTPATH"

export LOG_FILE=run_cli_wallet_img.log
# shellcheck source=common.sh
source "$SCRIPTPATH/common.sh"

log_exec_params "$@"

print_help () {
    echo "Usage: $0 <hived_docker_container_name> [OPTION[=VALUE]]... [<cli_wallet_option>]..."
    echo
    echo "Starts a cli_wallet inside an already running hived docker container."
    echo "OPTIONS:"
    echo "  --rpc-http-endpoint=<endpoint>  Specify the cli_wallet http endpoint. This should match the port specified for the cli_wallet when the hived container was started (defaults to 8093)."
    echo "  --rpc-http-allowip=ip_addr      Specify an IP address allowed to establish connections to the cli_wallet's API HTTP server."
    echo "  --help                          Displays this help screen and exits."
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

docker container exec -it "${CONTAINER_NAME}" /home/hived/bin/cli_wallet --wallet-file=/home/hived/datadir/wallet.json "${CLI_WALLET_ARGS[@]}"
