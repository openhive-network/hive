#! /bin/bash

set -euo pipefail

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"

INPUT_FILE="${1}"
DEFLATE_FILE="${2}"
B64_FILE="${3}"

zopfli -c --deflate -i1000 "${INPUT_FILE}" > "${DEFLATE_FILE}"
echo -n '"' >"${B64_FILE}"

# GNU coreutils accepts `base64 -w0 FILE`; BSD base64 (macOS) needs stdin
# redirection and emits no line wrapping by default. `tr -d '\n'` strips the
# trailing newline that GNU adds, keeping the output a single quoted token.
base64 < "${DEFLATE_FILE}" | tr -d '\n' >> "${B64_FILE}"

echo -n '"' >>"${B64_FILE}"
