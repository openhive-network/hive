#! /bin/bash -x

set -xeuo pipefail

HIVED_BINARY_PATH=${1}
BLOCK_LOG_UTIL_BINARY_PATH=${2}

# Note that 100k blocks fit within a single split block log file
# thus block_log.100k file can be used as both block_log & block_log_part.0001
SOURCE_100K_BLOCK_LOG_PATTERN="blockchain/block_log.100k"
# Same story with artifacts file, block_log.artifacts can be used as block_log_part.0001.artifacts here
SOURCE_ARTIFACTS_V_1_0_PATTERN="blockchain/block_log.artifacts.v_1_0"
SOURCE_ARTIFACTS_V_1_1_INTERRUPTED_PATTERN="blockchain/block_log.artifacts.v_1_1.interrupted_at_19313"

test_generate_artifacts_from_scratch_by_hived() {
  echo "Test 1 - generate artifacts from scratch by hived."
  local TEST_DATA_DIR="generate_from_scratch/by_hived"
  local TEST_BLOCKCHAIN_DIR="$TEST_DATA_DIR/blockchain"
  mkdir -p "$TEST_BLOCKCHAIN_DIR"
  cp "$SOURCE_100K_BLOCK_LOG_PATTERN" "$TEST_BLOCKCHAIN_DIR/block_log"
  # Note that both block_log.artifacts & block_log_part.0001.artifacts are created below
  # due to a combination of hived's default split setting & auto-split feature
  "$HIVED_BINARY_PATH" -d "$TEST_DATA_DIR" --replay --exit-before-sync --shared-file-size=1G
  "$BLOCK_LOG_UTIL_BINARY_PATH" --get-block-artifacts --block-log "$TEST_BLOCKCHAIN_DIR/block_log" --do-full-artifacts-verification-match-check --header-only
  "$BLOCK_LOG_UTIL_BINARY_PATH" --get-block-artifacts --block-log "$TEST_BLOCKCHAIN_DIR/block_log_part.0001" --do-full-artifacts-verification-match-check --header-only
}

test_generate_artifacts_override_old_file_by_hived() {
  echo "Test 2 - override old artifacts file and create a new one by hived."
  local TEST_DATA_DIR="override_old_file_and_generate_from_scratch/by_hived"
  local TEST_BLOCKCHAIN_DIR="$TEST_DATA_DIR/blockchain"
  mkdir -p "$TEST_BLOCKCHAIN_DIR"
  # Note that both block_log.artifacts & block_log_part.0001.artifacts are overridden below
  # due to a combination of hived's default split setting & auto-split feature
  cp "$SOURCE_100K_BLOCK_LOG_PATTERN" "$TEST_BLOCKCHAIN_DIR/block_log"
  cp "$SOURCE_ARTIFACTS_V_1_0_PATTERN" "$TEST_BLOCKCHAIN_DIR/block_log.artifacts"
  cp "$SOURCE_ARTIFACTS_V_1_0_PATTERN" "$TEST_BLOCKCHAIN_DIR/block_log_part.0001.artifacts"
  "$HIVED_BINARY_PATH" -d "$TEST_DATA_DIR" --replay --exit-before-sync --shared-file-size=1G
  "$BLOCK_LOG_UTIL_BINARY_PATH" --get-block-artifacts --block-log "$TEST_BLOCKCHAIN_DIR/block_log" --do-full-artifacts-verification-match-check --header-only
  "$BLOCK_LOG_UTIL_BINARY_PATH" --get-block-artifacts --block-log "$TEST_BLOCKCHAIN_DIR/block_log_part.0001" --do-full-artifacts-verification-match-check --header-only
}

test_resume_artifacts_generating_process_by_hived() {
  echo "Test 3 - Resume artifacts generating process by hived."
  local TEST_DATA_DIR="resume_artifacts_generating_process/by_hived"
  local TEST_BLOCKCHAIN_DIR="$TEST_DATA_DIR/blockchain"
  mkdir -p "$TEST_BLOCKCHAIN_DIR"
  cp "$SOURCE_100K_BLOCK_LOG_PATTERN" "$TEST_BLOCKCHAIN_DIR/block_log"
  cp "$SOURCE_ARTIFACTS_V_1_1_INTERRUPTED_PATTERN" "$TEST_BLOCKCHAIN_DIR/block_log.artifacts"
  "$HIVED_BINARY_PATH" -d "$TEST_DATA_DIR" --replay --exit-before-sync --shared-file-size=1G
  "$BLOCK_LOG_UTIL_BINARY_PATH" --get-block-artifacts --block-log "$TEST_BLOCKCHAIN_DIR/block_log" --do-full-artifacts-verification-match-check --header-only
}

test_generate_artifacts_from_scratch_by_block_log_util() {
  echo "Test 4 - generate artifacts from scratch by block_log_util."
  local TEST_BLOCKCHAIN_DIR="generate_from_scratch/by_block_log_util/blockchain"
  mkdir -p "$TEST_BLOCKCHAIN_DIR"
  cp "$SOURCE_100K_BLOCK_LOG_PATTERN" "$TEST_BLOCKCHAIN_DIR/block_log"
  "$BLOCK_LOG_UTIL_BINARY_PATH" --generate-artifacts --block-log "$TEST_BLOCKCHAIN_DIR/block_log"
  "$BLOCK_LOG_UTIL_BINARY_PATH" --get-block-artifacts --block-log "$TEST_BLOCKCHAIN_DIR/block_log" --do-full-artifacts-verification-match-check --header-only
  cp "$SOURCE_100K_BLOCK_LOG_PATTERN" "$TEST_BLOCKCHAIN_DIR/block_log_part.0001"
  "$BLOCK_LOG_UTIL_BINARY_PATH" --generate-artifacts --block-log "$TEST_BLOCKCHAIN_DIR/block_log_part.0001"
  "$BLOCK_LOG_UTIL_BINARY_PATH" --get-block-artifacts --block-log "$TEST_BLOCKCHAIN_DIR/block_log_part.0001" --do-full-artifacts-verification-match-check --header-only
}

test_generate_artifacts_override_old_file_by_block_log_util() {
  echo "Test 5 - override old artifacts file and create a new one by block_log_util."
  local TEST_BLOCKCHAIN_DIR="override_old_file_and_generate_from_scratch/by_block_log_util/blockchain"
  mkdir -p "$TEST_BLOCKCHAIN_DIR"
  cp "$SOURCE_100K_BLOCK_LOG_PATTERN" "$TEST_BLOCKCHAIN_DIR/block_log"
  cp "$SOURCE_ARTIFACTS_V_1_0_PATTERN" "$TEST_BLOCKCHAIN_DIR/block_log.artifacts"
  "$BLOCK_LOG_UTIL_BINARY_PATH" --generate-artifacts --block-log "$TEST_BLOCKCHAIN_DIR/block_log"
  "$BLOCK_LOG_UTIL_BINARY_PATH" --get-block-artifacts --block-log "$TEST_BLOCKCHAIN_DIR/block_log" --do-full-artifacts-verification-match-check --header-only
  cp "$SOURCE_100K_BLOCK_LOG_PATTERN" "$TEST_BLOCKCHAIN_DIR/block_log_part.0001"
  cp "$SOURCE_ARTIFACTS_V_1_0_PATTERN" "$TEST_BLOCKCHAIN_DIR/block_log_part.0001.artifacts"
  "$BLOCK_LOG_UTIL_BINARY_PATH" --generate-artifacts --block-log "$TEST_BLOCKCHAIN_DIR/block_log_part.0001"
  "$BLOCK_LOG_UTIL_BINARY_PATH" --get-block-artifacts --block-log "$TEST_BLOCKCHAIN_DIR/block_log_part.0001" --do-full-artifacts-verification-match-check --header-only
}

test_resume_artifacts_generating_process_by_block_log_util() {
  echo "Test 6 - Resume artifacts generating process by block_log_util."
  local TEST_BLOCKCHAIN_DIR="resume_artifacts_generating_process/by_block_log_util/blockchain"
  mkdir -p "$TEST_BLOCKCHAIN_DIR"
  cp "$SOURCE_100K_BLOCK_LOG_PATTERN" "$TEST_BLOCKCHAIN_DIR/block_log"
  cp "$SOURCE_ARTIFACTS_V_1_1_INTERRUPTED_PATTERN" "$TEST_BLOCKCHAIN_DIR/block_log.artifacts"
  "$BLOCK_LOG_UTIL_BINARY_PATH" --generate-artifacts --block-log "$TEST_BLOCKCHAIN_DIR/block_log"
  "$BLOCK_LOG_UTIL_BINARY_PATH" --get-block-artifacts --block-log "$TEST_BLOCKCHAIN_DIR/block_log" --do-full-artifacts-verification-match-check --header-only
  cp "$SOURCE_100K_BLOCK_LOG_PATTERN" "$TEST_BLOCKCHAIN_DIR/block_log_part.0001"
  cp "$SOURCE_ARTIFACTS_V_1_1_INTERRUPTED_PATTERN" "$TEST_BLOCKCHAIN_DIR/block_log_part.0001.artifacts"
  "$BLOCK_LOG_UTIL_BINARY_PATH" --generate-artifacts --block-log "$TEST_BLOCKCHAIN_DIR/block_log_part.0001"
  "$BLOCK_LOG_UTIL_BINARY_PATH" --get-block-artifacts --block-log "$TEST_BLOCKCHAIN_DIR/block_log_part.0001" --do-full-artifacts-verification-match-check --header-only
}

test_generate_artifacts_from_scratch_by_hived
test_generate_artifacts_override_old_file_by_hived
test_resume_artifacts_generating_process_by_hived
test_generate_artifacts_from_scratch_by_block_log_util
test_generate_artifacts_override_old_file_by_block_log_util
test_resume_artifacts_generating_process_by_block_log_util
