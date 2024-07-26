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
    echo "Number of files in directory where splitted block_log should be 8"
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
  "$BLOCK_LOG_UTIL_BINARY_PATH" --split -i "$BLOCK_LOG_TESTNET_PATTERN" -o "$TMP_SPLITTED_BLOCK_LOG_DIR" --from 400 --to 800

   files_count=$(ls -1 "$TMP_SPLITTED_BLOCK_LOG_DIR" | wc -l)
  if [ ${files_count} -ne 2 ]; then
    echo "Number of files in directory where splitted block_log should be 2 in case of extracting part of block_log"
    return 1
  fi

  "$BLOCK_LOG_UTIL_BINARY_PATH" --compare -i "$BLOCK_LOG_TESTNET_PATTERN" --second-block-log "$TMP_SPLITTED_BLOCK_LOG_DIR/block_log_part.0002" --from 401 --to 800

  echo "Test split --from 800"
  rm -r "$TMP_SPLITTED_BLOCK_LOG_DIR"
  "$BLOCK_LOG_UTIL_BINARY_PATH" --split -i "$BLOCK_LOG_TESTNET_PATTERN" -o "$TMP_SPLITTED_BLOCK_LOG_DIR" --from 800
  files_count=$(ls -1 "$TMP_SPLITTED_BLOCK_LOG_DIR" | wc -l)
  if [ ${files_count} -ne 4 ]; then
    echo "Number of files in directory where splitted block_log should be 4 in this case"
    return 1
  fi

  "$BLOCK_LOG_UTIL_BINARY_PATH" --compare -i "$BLOCK_LOG_TESTNET_PATTERN" --second-block-log "$TMP_SPLITTED_BLOCK_LOG_DIR/block_log_part.0003" --from 801 --to 1200
  "$BLOCK_LOG_UTIL_BINARY_PATH" --compare -i "$BLOCK_LOG_TESTNET_PATTERN" --second-block-log "$TMP_SPLITTED_BLOCK_LOG_DIR/block_log_part.0004" --from 1201

  echo "Test split --to 800"
  rm -r "$TMP_SPLITTED_BLOCK_LOG_DIR"
  "$BLOCK_LOG_UTIL_BINARY_PATH" --split -i "$BLOCK_LOG_TESTNET_PATTERN" -o "$TMP_SPLITTED_BLOCK_LOG_DIR" --to 800
  files_count=$(ls -1 "$TMP_SPLITTED_BLOCK_LOG_DIR" | wc -l)
  if [ ${files_count} -ne 4 ]; then
    echo "Number of files in directory where splitted block_log should be 4 in this case"
    return 1
  fi

  "$BLOCK_LOG_UTIL_BINARY_PATH" --compare -i "$BLOCK_LOG_TESTNET_PATTERN" --second-block-log "$TMP_SPLITTED_BLOCK_LOG_DIR/block_log_part.0001"  --to 400
  "$BLOCK_LOG_UTIL_BINARY_PATH" --compare -i "$BLOCK_LOG_TESTNET_PATTERN" --second-block-log "$TMP_SPLITTED_BLOCK_LOG_DIR/block_log_part.0002" --to 800 --from 401

  echo "test --files-count - positive value"
  rm -r "$TMP_SPLITTED_BLOCK_LOG_DIR"
  "$BLOCK_LOG_UTIL_BINARY_PATH" --split -i "$BLOCK_LOG_TESTNET_PATTERN" -o "$TMP_SPLITTED_BLOCK_LOG_DIR" --files-count 3
  files_count=$(ls -1 "$TMP_SPLITTED_BLOCK_LOG_DIR" | wc -l)
  if [ ${files_count} -ne 6 ]; then
    echo "Number of files in directory where splitted block_log should be 6 in this case"
    return 1
  fi

  "$BLOCK_LOG_UTIL_BINARY_PATH" --compare -i "$BLOCK_LOG_TESTNET_PATTERN" --second-block-log "$TMP_SPLITTED_BLOCK_LOG_DIR/block_log_part.0001"  --to 400
  "$BLOCK_LOG_UTIL_BINARY_PATH" --compare -i "$BLOCK_LOG_TESTNET_PATTERN" --second-block-log "$TMP_SPLITTED_BLOCK_LOG_DIR/block_log_part.0002" --to 800 --from 401
  "$BLOCK_LOG_UTIL_BINARY_PATH" --compare -i "$BLOCK_LOG_TESTNET_PATTERN" --second-block-log "$TMP_SPLITTED_BLOCK_LOG_DIR/block_log_part.0003" --to 1200 --from 801

  echo "test --files-count - negative value"
  rm -r "$TMP_SPLITTED_BLOCK_LOG_DIR"
  "$BLOCK_LOG_UTIL_BINARY_PATH" --split -i "$BLOCK_LOG_TESTNET_PATTERN" -o "$TMP_SPLITTED_BLOCK_LOG_DIR" --files-count -3
  files_count=$(ls -1 "$TMP_SPLITTED_BLOCK_LOG_DIR" | wc -l)
  if [ ${files_count} -ne 6 ]; then
    echo "Number of files in directory where splitted block_log should be 6 in this case"
    return 1
  fi

  "$BLOCK_LOG_UTIL_BINARY_PATH" --compare -i "$BLOCK_LOG_TESTNET_PATTERN" --second-block-log "$TMP_SPLITTED_BLOCK_LOG_DIR/block_log_part.0002" --from 401 --to 800
  "$BLOCK_LOG_UTIL_BINARY_PATH" --compare -i "$BLOCK_LOG_TESTNET_PATTERN" --second-block-log "$TMP_SPLITTED_BLOCK_LOG_DIR/block_log_part.0003" --to 1200 --from 801
  "$BLOCK_LOG_UTIL_BINARY_PATH" --compare -i "$BLOCK_LOG_TESTNET_PATTERN" --second-block-log "$TMP_SPLITTED_BLOCK_LOG_DIR/block_log_part.0004" --from 1201

  echo "test --files-count with --from"
  rm -r "$TMP_SPLITTED_BLOCK_LOG_DIR"
  "$BLOCK_LOG_UTIL_BINARY_PATH" --split -i "$BLOCK_LOG_TESTNET_PATTERN" -o "$TMP_SPLITTED_BLOCK_LOG_DIR" --files-count 2 --from 400
  files_count=$(ls -1 "$TMP_SPLITTED_BLOCK_LOG_DIR" | wc -l)
  if [ ${files_count} -ne 4 ]; then
    echo "Number of files in directory where splitted block_log should be 4 in this case"
    return 1
  fi

  "$BLOCK_LOG_UTIL_BINARY_PATH" --compare -i "$BLOCK_LOG_TESTNET_PATTERN" --second-block-log "$TMP_SPLITTED_BLOCK_LOG_DIR/block_log_part.0002" --from 401 --to 800
  "$BLOCK_LOG_UTIL_BINARY_PATH" --compare -i "$BLOCK_LOG_TESTNET_PATTERN" --second-block-log "$TMP_SPLITTED_BLOCK_LOG_DIR/block_log_part.0003" --to 1200 --from 801

  echo "test --files-count with --to"
  rm -r "$TMP_SPLITTED_BLOCK_LOG_DIR"
  "$BLOCK_LOG_UTIL_BINARY_PATH" --split -i "$BLOCK_LOG_TESTNET_PATTERN" -o "$TMP_SPLITTED_BLOCK_LOG_DIR" --files-count 2 --to 1200
  files_count=$(ls -1 "$TMP_SPLITTED_BLOCK_LOG_DIR" | wc -l)
  if [ ${files_count} -ne 4 ]; then
    echo "Number of files in directory where splitted block_log should be 4 in this case"
    return 1
  fi

  "$BLOCK_LOG_UTIL_BINARY_PATH" --compare -i "$BLOCK_LOG_TESTNET_PATTERN" --second-block-log "$TMP_SPLITTED_BLOCK_LOG_DIR/block_log_part.0002" --from 401 --to 800
  "$BLOCK_LOG_UTIL_BINARY_PATH" --compare -i "$BLOCK_LOG_TESTNET_PATTERN" --second-block-log "$TMP_SPLITTED_BLOCK_LOG_DIR/block_log_part.0003" --to 1200 --from 801
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

test_split_and_merge_block_log_errors() {
  TMP_DIR="test_tmp_dir"

  if "$BLOCK_LOG_UTIL_BINARY_PATH" --split -i "$BLOCK_LOG_TESTNET_PATTERN" -o "$TMP_DIR" --files-count 5 ; then
    echo "This command should fail. No more than 4 parts could be created."
    return 1
  fi

  if "$BLOCK_LOG_UTIL_BINARY_PATH" --split -i "$BLOCK_LOG_TESTNET_PATTERN" -o "$TMP_DIR" --from 1600; then
    echo "This command should fail. Above head block number"
    return 1
  fi

  if "$BLOCK_LOG_UTIL_BINARY_PATH" --split -i "$BLOCK_LOG_TESTNET_PATTERN" -o "$TMP_DIR" --to 1600; then
    echo "This command should fail. Above head block number"
    return 1
  fi

  if "$BLOCK_LOG_UTIL_BINARY_PATH" --split -i "$BLOCK_LOG_TESTNET_PATTERN" -o "$TMP_DIR" --from 400 --files-count 4; then
    echo "This command should fail. No more than 3 parts could be created."
    return 1
  fi

  if "$BLOCK_LOG_UTIL_BINARY_PATH" --split -i "$BLOCK_LOG_TESTNET_PATTERN" -o "$TMP_DIR" --to 1200 --files-count 4; then
    echo "This command should fail. No more than 3 parts could be created."
    return 1
  fi

  if "$BLOCK_LOG_UTIL_BINARY_PATH" --split -i "$BLOCK_LOG_TESTNET_PATTERN" -o "$TMP_DIR" --from 400 --to 1200 --files-count 2 ; then
    echo "This command should fail. --from --to and --files-count cannot be used together at once."
    return 1
  fi

  if "$BLOCK_LOG_UTIL_BINARY_PATH" --split -i "$BLOCK_LOG_TESTNET_PATTERN" -o "$TMP_DIR" --to 99; then
    echo "This command should fail. In testnet case, --to must be multiplicity of 400."
    return 1
  fi

  if "$BLOCK_LOG_UTIL_BINARY_PATH" --split -i "$BLOCK_LOG_TESTNET_PATTERN" -o "$TMP_DIR" --from 99; then
    echo "This command should fail. In testnet case, --from must be multiplicity of 400."
    return 1
  fi

  if "$BLOCK_LOG_UTIL_BINARY_PATH" --merge-block-logs -i "$SPLITTED_BLOCK_LOGS_DIRECTORY" -o "$TMP_DIR" -n 1312; then
    echo "This command should fail. Above head block number"
    return 1
  fi

  rm "$SPLITTED_BLOCK_LOGS_DIRECTORY/block_log_part.0003"
  rm "$SPLITTED_BLOCK_LOGS_DIRECTORY/block_log_part.0003.artifacts"
  if "$BLOCK_LOG_UTIL_BINARY_PATH" --merge-block-logs -i "$SPLITTED_BLOCK_LOGS_DIRECTORY" -o "$TMP_DIR"; then
    echo "This command should fail. Missing block_log_part.0003"
    return 1
  fi

  if "$BLOCK_LOG_UTIL_BINARY_PATH" --merge-block-logs -i "$TMP_DIR" -o "$TMP_DIR"; then
    echo "This command should fail. Nothing in tmp directory."
    return 1
  fi
}

test_split_block_log
test_merge_splitted_block_log
test_split_and_merge_block_log_errors