#! /bin/bash -x

set -xeuo pipefail

SOURCE_100K_BLOCK_LOG_PATTERN="blockchain/block_log.100k"
SOURCE_ARTIFACTS_V_1_0_PATTERN="blockchain/block_log.artifacts.v_1_0"
SOURCE_ARTIFACTS_V_1_1_INTERRUPTED_PATTERN="blockchain/block_log.artifacts.v_1_1.interrupted_at_19313"

test_generate_artifacts_from_scratch() {
  echo "Test 1 - generate artifacts from scratch."
  local TEST_DATA_DIR="generate_from_scratch"
  local TEST_BLOCKCHAIN_DIR="$TEST_DATA_DIR/blockchain"
  mkdir -p "$TEST_BLOCKCHAIN_DIR"
  cp "$SOURCE_100K_BLOCK_LOG_PATTERN" "$TEST_BLOCKCHAIN_DIR/block_log"
  "$CI_PROJECT_DIR/$BINARY_CACHE_PATH/hived" -d="$TEST_DATA_DIR" --replay --exit-before-sync
}

test_generate_artifacts_override_old_file() {
  echo "Test 2 - override old artifacts file and create a new one."
  local TEST_DATA_DIR="override_old_file_and_generate_from_scratch"
  local TEST_BLOCKCHAIN_DIR="$TEST_DATA_DIR/blockchain"
  mkdir -p "$TEST_BLOCKCHAIN_DIR"
  cp "$SOURCE_100K_BLOCK_LOG_PATTERN" "$TEST_BLOCKCHAIN_DIR/block_log"
  cp "$SOURCE_ARTIFACTS_V_1_0_PATTERN" "$TEST_BLOCKCHAIN_DIR/block_log.artifacts"
  "$CI_PROJECT_DIR/$BINARY_CACHE_PATH/hived" -d="$TEST_DATA_DIR" --replay --exit-before-sync
}

test_resume_artifacts_generating_process() {
  echo "Test 3 - Resume artifacts generating process"
  local TEST_DATA_DIR="resume_artifacts_generating_process"
  local TEST_BLOCKCHAIN_DIR="$TEST_DATA_DIR/blockchain"
  mkdir -p "$TEST_BLOCKCHAIN_DIR"
  cp "$SOURCE_100K_BLOCK_LOG_PATTERN" "$TEST_BLOCKCHAIN_DIR/block_log"
  cp "$SOURCE_ARTIFACTS_V_1_1_INTERRUPTED_PATTERN" "$TEST_BLOCKCHAIN_DIR/block_log.artifacts"
  "$CI_PROJECT_DIR/$BINARY_CACHE_PATH/hived" -d="$TEST_DATA_DIR" --replay --exit-before-sync
}

test_generate_artifacts_from_scratch
test_generate_artifacts_override_old_file
test_resume_artifacts_generating_process