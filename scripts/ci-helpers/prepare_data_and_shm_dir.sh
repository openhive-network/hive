#! /bin/bash
set -xeuo pipefail

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


if [ -n "$BLOCK_LOG_SOURCE_DIR" ] && [ -e $BLOCK_LOG_SOURCE_DIR/block_log ];
then
  mkdir -p $DATA_BASE_DIR/datadir/blockchain
  cp $BLOCK_LOG_SOURCE_DIR/block_log $DATA_BASE_DIR/datadir/blockchain/block_log
  if [ -e $BLOCK_LOG_SOURCE_DIR/block_log.artifacts ];
  then
    cp $BLOCK_LOG_SOURCE_DIR/block_log.artifacts $DATA_BASE_DIR/datadir/blockchain/block_log.artifacts
  fi
fi


if [ -n "$CONFIG_INI_SOURCE" ];
then
  cp "$CONFIG_INI_SOURCE" $DATA_BASE_DIR/datadir/config.ini
fi
