#! /bin/bash

set -euo pipefail

EXTENDED_BLOCK_LOG_IMAGE=${EXTENDED_BLOCK_LOG_IMAGE:-""}
EXPORT_PATH=${EXPORT_PATH:-""}
IMAGE_PATH=${IMAGE_PATH:-"/home/hived/bin/"}

print_help () {
cat <<-EOF
  Usage: $0 <image> <local-directory> [OPTION[=VALUE]]...

  Exports data from a Docker image to a local directory
  OPTIONS:
    --image-path=PATH         Path inside the Docker image that is to be exported (default: /home/hived/bin/).
    --help,-h,-?              Display this help screen and exit
EOF
}

while [ $# -gt 0 ]; do
  case "$1" in
    --image-path=*)
        IMAGE_PATH="${1#*=}"
        ;;
    --help|-h|-\?)
        print_help
        exit 0
        ;;
    -*)
        echo "ERROR: '$1' is not a valid option"
        exit 1
        ;;
    *)
        if [ -z "${IMAGE_NAME:-}" ];
        then
          IMAGE_NAME="$1"
        elif [ -z "${LOCAL_PATH:-}" ];
        then
          LOCAL_PATH=$1
        else
          echo "ERROR: '$1' is not a valid positional argument"
          echo
          print_help
          exit 2
        fi
        ;;
    esac
    shift
done

IMAGE_NAME=${IMAGE_NAME:-${EXTENDED_BLOCK_LOG_IMAGE:?"Missing Docker image tag (use --help to see options)"}}
LOCAL_PATH=${LOCAL_PATH:-${EXPORT_PATH:?"Missing target directory (use --help to see options)"}}

echo "Attempting to export data from Docker image ${IMAGE_NAME} directory ${IMAGE_PATH} into host directory: ${LOCAL_PATH}"

docker build -o "${LOCAL_PATH}" - << EOF
    FROM scratch
    COPY --from=${IMAGE_NAME} ${IMAGE_PATH} /
EOF
