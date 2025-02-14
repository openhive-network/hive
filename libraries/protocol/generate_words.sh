#! /bin/bash

set -euo pipefail

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"

INPUT_FILE="${1}"
DEFLATE_FILE="${2}"
B64_FILE="${3}"

zopfli -c --deflate -i1000 "${INPUT_FILE}" > "${DEFLATE_FILE}"
echo -n '"' >"${B64_FILE}"

base64 -w0 "${DEFLATE_FILE}" >> "${B64_FILE}"

echo -n '"' >>"${B64_FILE}"
