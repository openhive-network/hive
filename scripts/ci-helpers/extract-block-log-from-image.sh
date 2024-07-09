#!/bin/bash

set -e

EXTENDED_BLOCK_LOG_IMAGE=${EXTENDED_BLOCK_LOG_IMAGE:?}
EXPORT_PATH=${EXPORT_PATH:-"$(pwd)"}

docker build -o "${EXPORT_PATH}" - << EOF
    FROM scratch
    COPY --from=${EXTENDED_BLOCK_LOG_IMAGE} /blockchain/ /
EOF