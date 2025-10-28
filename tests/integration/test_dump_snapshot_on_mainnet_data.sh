#! /bin/bash -x

set -xeuo pipefail

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
SRC_DIR="${SCRIPTPATH}/../.."
DATA_DIR="${SRC_DIR}/datadir/"

mkdir -vp "${DATA_DIR}"

echo "DD ${DATA_DIR}"
echo "222222 ${SRC_DIR}"


# Sprawdzenie, czy podano wymagane argumenty
if [ "$#" -lt 2 ]; then
    echo "Użycie: $0 <hived_image_tag> <data_dir> [opcje...]"
    exit 1
fi

HIVED_IMAGE_TAG=$1
DATA_DIR=$2
shift 2 # Usunięcie pierwszych dwóch argumentów

# Uruchomienie skryptu build_and_setup_exchange_instance.sh
./scripts/build_and_setup_exchange_instance.sh "$HIVED_IMAGE_TAG" --data-dir="$DATA_DIR" "$@"