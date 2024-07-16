#! /bin/bash -x

set -xeuo pipefail

BLOCK_LOG_UTIL_BINARY_PATH=${1}

BLOCK_LOG_TESTNET_PATTERN="block_log_testnet_pattern/block_log"
SPLITTED_BLOCK_LOGS_DIRECTORY="splitted_block_logs"

mkdir -p "$SPLITTED_BLOCK_LOGS_DIRECTORY"

test_split_block_log() {
  echo "Test 1 - split block_log file."
  echo "Generate artifacts for block_log"
  "$BLOCK_LOG_UTIL_BINARY_PATH" --generate-artifacts -i "$BLOCK_LOG_TESTNET_PATTERN"
  echo "Verify if artifacts are good"
  "$BLOCK_LOG_UTIL_BINARY_PATH" --get-block-artifacts --block-log "$BLOCK_LOG_TESTNET_PATTERN" --do-full-artifacts-verification-match-check --header-only
  echo "Split block_log"
  "$BLOCK_LOG_UTIL_BINARY_PATH" --split -i "$BLOCK_LOG_TESTNET_PATTERN" -o "$SPLITTED_BLOCK_LOGS_DIRECTORY"
  echo "In this case, 4 block_log files and 4 artifacts file should be created"
  local files_count=$(ls -1 "$SPLITTED_BLOCK_LOGS_DIRECTORY" | wc -l)
  if [ ${files_count} -ne 8 ]; then
    echo "Number of files in directory where splitted block_log should be should be 8"
    return 1
  fi

  echo "Compare pattern block_log with splitted block_logs"
  "$BLOCK_LOG_UTIL_BINARY_PATH" --compare -i "$BLOCK_LOG_TESTNET_PATTERN" --second-block-log "$SPLITTED_BLOCK_LOGS_DIRECTORY/block_log_part.0001" --to 400
  "$BLOCK_LOG_UTIL_BINARY_PATH" --compare -i "$BLOCK_LOG_TESTNET_PATTERN" --second-block-log "$SPLITTED_BLOCK_LOGS_DIRECTORY/block_log_part.0002" --from 401 --to 800
  "$BLOCK_LOG_UTIL_BINARY_PATH" --compare -i "$BLOCK_LOG_TESTNET_PATTERN" --second-block-log "$SPLITTED_BLOCK_LOGS_DIRECTORY/block_log_part.0003" --from 801 --to 1200
  "$BLOCK_LOG_UTIL_BINARY_PATH" --compare -i "$BLOCK_LOG_TESTNET_PATTERN" --second-block-log "$SPLITTED_BLOCK_LOGS_DIRECTORY/block_log_part.0004" --from 1201

  echo "rm all artifacts from splitted block_logs dir, regenerate them and then compare again"
  rm "$SPLITTED_BLOCK_LOGS_DIRECTORY"/*.artifacts
  "$BLOCK_LOG_UTIL_BINARY_PATH" --generate-artifacts -i "$SPLITTED_BLOCK_LOGS_DIRECTORY/block_log_part.0001"
  "$BLOCK_LOG_UTIL_BINARY_PATH" --generate-artifacts -i "$SPLITTED_BLOCK_LOGS_DIRECTORY/block_log_part.0002"
  "$BLOCK_LOG_UTIL_BINARY_PATH" --generate-artifacts -i "$SPLITTED_BLOCK_LOGS_DIRECTORY/block_log_part.0003"
  "$BLOCK_LOG_UTIL_BINARY_PATH" --generate-artifacts -i "$SPLITTED_BLOCK_LOGS_DIRECTORY/block_log_part.0004"

  "$BLOCK_LOG_UTIL_BINARY_PATH" --get-block-artifacts -i "$SPLITTED_BLOCK_LOGS_DIRECTORY/block_log_part.0001" --header-only --do-full-artifacts-verification-match-check
  "$BLOCK_LOG_UTIL_BINARY_PATH" --get-block-artifacts -i "$SPLITTED_BLOCK_LOGS_DIRECTORY/block_log_part.0002" --header-only --do-full-artifacts-verification-match-check
  "$BLOCK_LOG_UTIL_BINARY_PATH" --get-block-artifacts -i "$SPLITTED_BLOCK_LOGS_DIRECTORY/block_log_part.0003" --header-only --do-full-artifacts-verification-match-check
  "$BLOCK_LOG_UTIL_BINARY_PATH" --get-block-artifacts -i "$SPLITTED_BLOCK_LOGS_DIRECTORY/block_log_part.0004" --header-only --do-full-artifacts-verification-match-check

  echo "Compare pattern block_log with splitted block_logs"
  "$BLOCK_LOG_UTIL_BINARY_PATH" --compare -i "$BLOCK_LOG_TESTNET_PATTERN" --second-block-log "$SPLITTED_BLOCK_LOGS_DIRECTORY/block_log_part.0001" --to 400
  "$BLOCK_LOG_UTIL_BINARY_PATH" --compare -i "$BLOCK_LOG_TESTNET_PATTERN" --second-block-log "$SPLITTED_BLOCK_LOGS_DIRECTORY/block_log_part.0002" --from 401 --to 800
  "$BLOCK_LOG_UTIL_BINARY_PATH" --compare -i "$BLOCK_LOG_TESTNET_PATTERN" --second-block-log "$SPLITTED_BLOCK_LOGS_DIRECTORY/block_log_part.0003" --from 801 --to 1200
  "$BLOCK_LOG_UTIL_BINARY_PATH" --compare -i "$BLOCK_LOG_TESTNET_PATTERN" --second-block-log "$SPLITTED_BLOCK_LOGS_DIRECTORY/block_log_part.0004" --from 1201

  echo "Try to extract part of block_log"
  local TMP_SPLITTED_BLOCK_LOG_DIR="tmp_limited_splitted_block_log"
  mkdir -p "$TMP_SPLITTED_BLOCK_LOG_DIR"
  "$BLOCK_LOG_UTIL_BINARY_PATH" --split -i "$BLOCK_LOG_TESTNET_PATTERN" -o "$TMP_SPLITTED_BLOCK_LOG_DIR" --from 400 --to 800

  local files_count_second_split=$(ls -1 "$TMP_SPLITTED_BLOCK_LOG_DIR" | wc -l)
  if [ ${files_count_second_split} -ne 2 ]; then
    echo "Number of files in directory where splitted block_log should be should be 2 in case of extracting part of block_log"
    return 1
  fi

  "$BLOCK_LOG_UTIL_BINARY_PATH" --compare -i "$BLOCK_LOG_TESTNET_PATTERN" --second-block-log "$TMP_SPLITTED_BLOCK_LOG_DIR/block_log_part.0002" --from 401 --to 800
}

test_merge_splitted_block_log() {
  echo "Test 2 - merge splitted block_log into one file"
  local MERGED_BLOCK_LOG_DIRECTORY="MERGED_BLOCK_LOG"
  mkdir -p "$MERGED_BLOCK_LOG_DIRECTORY"
  "$BLOCK_LOG_UTIL_BINARY_PATH" --merge-block-logs -i "$SPLITTED_BLOCK_LOGS_DIRECTORY" -o "$MERGED_BLOCK_LOG_DIRECTORY"
  "$BLOCK_LOG_UTIL_BINARY_PATH" --compare -i "$BLOCK_LOG_TESTNET_PATTERN" --second-block-log "$MERGED_BLOCK_LOG_DIRECTORY/block_log"
  rm "$MERGED_BLOCK_LOG_DIRECTORY/block_log"
  rm "$MERGED_BLOCK_LOG_DIRECTORY/block_log.artifacts"
  echo "Try to merge up to 700 blocks"
  "$BLOCK_LOG_UTIL_BINARY_PATH" --merge-block-logs -i "$SPLITTED_BLOCK_LOGS_DIRECTORY" -o "$MERGED_BLOCK_LOG_DIRECTORY" -n 700
  "$BLOCK_LOG_UTIL_BINARY_PATH" --compare -i "$BLOCK_LOG_TESTNET_PATTERN" --second-block-log "$MERGED_BLOCK_LOG_DIRECTORY/block_log" --to 700
  local TMP_PATTERN_BLOCK_LOG_DIR="tmp_directory_for_truncated_block_log"
  mkdir -p "$TMP_PATTERN_BLOCK_LOG_DIR"
  cp "$BLOCK_LOG_TESTNET_PATTERN" "$TMP_PATTERN_BLOCK_LOG_DIR/block_log"
  "$BLOCK_LOG_UTIL_BINARY_PATH" --generate-artifacts -i "$TMP_PATTERN_BLOCK_LOG_DIR/block_log"
  "$BLOCK_LOG_UTIL_BINARY_PATH" --truncate -i "$TMP_PATTERN_BLOCK_LOG_DIR/block_log" -f --block-number 700
  "$BLOCK_LOG_UTIL_BINARY_PATH" --compare -i "$MERGED_BLOCK_LOG_DIRECTORY/block_log" --second-block-log "$TMP_PATTERN_BLOCK_LOG_DIR/block_log"
}

test_split_block_log
test_merge_splitted_block_log