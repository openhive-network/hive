#! /bin/bash
set -xeuo pipefail

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
SCRIPTSDIR="$SCRIPTPATH/.."
SRCROOTDIR="$SCRIPTSDIR/.."

LOG_FILE=build_data4commit.log
source "$SCRIPTSDIR/common.sh"

COMMIT=${1:?"Missing arg 1 to specify COMMIT"}
shift
REGISTRY=${1:?"Missing arg #2 to specify target container registry"}
shift
BRANCH="master"

BUILD_IMAGE_TAG=$COMMIT

do_clone "$BRANCH" "./hive-${COMMIT}" https://gitlab.syncad.com/hive/hive.git "$COMMIT"

"$SCRIPTSDIR/ci-helpers/build_data2.sh" "$BUILD_IMAGE_TAG" "./hive-${COMMIT}" "$REGISTRY" "$@"

