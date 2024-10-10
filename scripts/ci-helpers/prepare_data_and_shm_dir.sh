#! /bin/bash
set -xeuo pipefail
shopt -s nullglob

while [ $# -gt 0 ]; do
  case "$1" in
    --data-base-dir=*)
        DATA_BASE_DIR="${1#*=}"
        echo "using DATA_BASE_DIR $DATA_BASE_DIR"
        ;;
    --block-log-source-dir=*)
        BLOCK_LOG_SOURCE_DIR="${1#*=}"
        echo "block-log-source-dir $BLOCK_LOG_SOURCE_DIR"
        ;;
    --config-ini-source=*)
        CONFIG_INI_SOURCE="${1#*=}"
        echo "config-ini $CONFIG_INI_SOURCE"
        ;;
    *)
        echo "ERROR: '$1' is not a valid option/positional argument"
        echo
        exit 2
    esac
    shift
done


if [ -z $DATA_BASE_DIR ];
then
  echo "No DATA_BASE_DIR directory privided, skipping this script"
  exit 1
else
  mkdir -p $DATA_BASE_DIR/datadir
  mkdir -p $DATA_BASE_DIR/shm_dir
fi

function handle_single_file_of_block_log() {
  local FILE_PATH=$1
  local FILE_NAME=$(basename -- "$FILE_PATH")

  mkdir -p $DATA_BASE_DIR/datadir/blockchain

  if [ -n "${HIVE_NETWORK_TYPE+x}" ] && [ "$HIVE_NETWORK_TYPE" = mirrornet ];
  then
    echo "creating copy of block log file as mirrornet block log can't be shared between pipelines"
    cp "$FILE_PATH" "$DATA_BASE_DIR/datadir/blockchain/"
  else
    local BLOCK_LOG_TARGET_DIR="$DATA_BASE_DIR/..$BLOCK_LOG_SOURCE_DIR"
    echo "using $BLOCK_LOG_TARGET_DIR/$FILE_NAME as hardlink target"
    mkdir -p "$BLOCK_LOG_TARGET_DIR"
    cp -u "$FILE_PATH" "$BLOCK_LOG_TARGET_DIR/$FILE_NAME"
    echo "creating hardlink of $BLOCK_LOG_TARGET_DIR/$FILE_NAME in $DATA_BASE_DIR/datadir/blockchain/"
    ln "$BLOCK_LOG_TARGET_DIR/$FILE_NAME" "$DATA_BASE_DIR/datadir/blockchain/$FILE_NAME"
  fi

  if [ -e $FILE_PATH.artifacts ];
  then
    cp $FILE_PATH.artifacts $DATA_BASE_DIR/datadir/blockchain/$FILE_NAME.artifacts
  fi
}

if [ -n "$BLOCK_LOG_SOURCE_DIR" ]; then
  if [ -e $BLOCK_LOG_SOURCE_DIR/block_log ]; then
    handle_single_file_of_block_log "$BLOCK_LOG_SOURCE_DIR/block_log"
  fi
  if ls $BLOCK_LOG_SOURCE_DIR/block_log_part.???? 1>/dev/null 2>&1; then
    for TARGET_FILE in $BLOCK_LOG_SOURCE_DIR/block_log_part.????; do
      handle_single_file_of_block_log "$TARGET_FILE"
    done
  fi
fi


if [ -n "$CONFIG_INI_SOURCE" ];
then
  cp "$CONFIG_INI_SOURCE" $DATA_BASE_DIR/datadir/config.ini
fi
