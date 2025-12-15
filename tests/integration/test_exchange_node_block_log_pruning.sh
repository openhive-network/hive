#! /bin/bash -x

set -xeuo pipefail

################################################################################
# Test: Exchange instance with pruned block log
################################################################################
#
# Purpose: Test pruned block log functionality for exchange nodes.
#          Automatically removes old block_log parts to save disk space.
#
# Test timeline:
#
#   0 ──────────── 1.1M ──────────────────── 5.1M ───────> blocks
#       STEP 1       ↓         STEP 3           ↓
#      (sync)    SNAPSHOT   (load+sync)      STOP
#                 STEP 2
#
# Test flow:
#   STEP 1: Initial sync to 1.1M (no pruning, split=9999)
#           - Sync from genesis to block 1.1M
#           - Checkpoint guarantees LIB at block 1.1M with correct block_id
#           - Node exits only when LIB reaches checkpoint (safe shutdown)
#           - Keep all block_log parts (split=9999)
#
#   STEP 2: Dump snapshot at 1.1M
#           - Create state snapshot at block 1.1M
#           - Saves chainbase state (accounts, witnesses, etc.)
#
#   STEP 3: Load snapshot + sync to 5.1M (with pruning, split=1)
#           - Load snapshot (start from block 1.1M state)
#           - Sync from 1.1M to 5.1M (4M new blocks)
#           - Automatic pruning: delete old parts, keep only split=1
#
# Block log parts after STEP 3 (split=1 keeps only 1 completed part):
#
#   Part 0001 [0-1M]      ❌ PRUNED (deleted during sync)
#   Part 0002 [1M-2M]     ❌ PRUNED (deleted during sync)
#   Part 0003 [2M-3M]     ❌ PRUNED (deleted during sync)
#   Part 0004 [3M-4M]     ❌ PRUNED (deleted during sync)
#   Part 0005 [4M-5M]     ✅ KEPT (oldest_kept_part, split=1)
#   Part 0006 [5M-6M]     ✅ KEPT (current_part)
#             ↑
#         TEST_BLOCK = 4.1M (should be accessible via API)
#
# Verifications:
#   1. Node syncs to target block (5.1M)
#   2. Block in kept range (4.1M) is accessible via API
#   3. Old parts (0001-0004) are deleted
#   4. Recent parts (0005-0006) exist
#
################################################################################

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
SRC_DIR="$(realpath "${SCRIPTPATH}/../..")"
SCRIPTS_PATH="${SCRIPTS_PATH:-${SRC_DIR}/scripts}"
CI_PROJECT_DIR="${CI_PROJECT_DIR:-$SRC_DIR}"
SNAPSHOT_NAME="test_snapshot"

source "$SCRIPTPATH/conftest.sh"

# Configurable test parameters
INITIAL_SYNC_BLOCK=${INITIAL_SYNC_BLOCK:-1100000}    # Initial sync block (default 1.1M)
TARGET_SYNC_BLOCK=${TARGET_SYNC_BLOCK:-5100000}      # Target sync block (default 5.1M)
BLOCK_LOG_SPLIT=1                                     # Fixed: always keep only 1 completed part
BLOCKS_PER_PART=1000000                               # Blocks per part file (1M - constant)

# Checkpoint for initial sync (block_id is mainnet-specific for block 1.1M)
CHECKPOINT_BLOCK=${INITIAL_SYNC_BLOCK}
CHECKPOINT_BLOCK_ID="0010c8e0e174e36ccf1143a1d1cda30497db089a"  # Mainnet block 1100000

# Calculate part numbers based on block ranges
INITIAL_PART=$(( (INITIAL_SYNC_BLOCK - 1) / BLOCKS_PER_PART + 1 ))
TARGET_PART=$(( (TARGET_SYNC_BLOCK - 1) / BLOCKS_PER_PART + 1 ))
CURRENT_PART=$(( TARGET_PART ))
OLDEST_KEPT_PART=$(( CURRENT_PART - BLOCK_LOG_SPLIT ))

# Calculate which block to test (should be in oldest kept part)
TEST_BLOCK=$(( (OLDEST_KEPT_PART - 1) * BLOCKS_PER_PART + 100000 ))

echo "================================================================================"
echo "  Exchange Instance with Pruned Block Log Test"
echo "================================================================================"
echo ""
echo "Configuration:"
echo "  Initial sync block: $INITIAL_SYNC_BLOCK (part $INITIAL_PART)"
echo "  Target sync block:  $TARGET_SYNC_BLOCK (part $TARGET_PART)"
echo "  Block log split:    $BLOCK_LOG_SPLIT (keep only $BLOCK_LOG_SPLIT completed part)"
echo "  Test block:         $TEST_BLOCK"
echo ""
echo "Expected block log parts after sync:"
echo "  Part 0001 [0-1M]      ❌ PRUNED"
echo "  Part 0002 [1M-2M]     ❌ PRUNED"
echo "  Part 0003 [2M-3M]     ❌ PRUNED"
echo "  Part 0004 [3M-4M]     ❌ PRUNED"
echo "  Part $(printf "%04d" $OLDEST_KEPT_PART) [4M-5M]     ✅ KEPT (oldest_kept_part)"
echo "  Part $(printf "%04d" $CURRENT_PART) [5M-6M]     ✅ KEPT (current_part)"
echo ""
echo "================================================================================"

if [ "$#" -lt 2 ]; then
    echo "Usage: $0 <hived_image_name> <data_dir> [options...]"
    echo ""
    echo "Environment variables:"
    echo "  INITIAL_SYNC_BLOCK  - Initial sync block (default: 1100000)"
    echo "  TARGET_SYNC_BLOCK   - Target sync block (default: 5100000)"
    echo ""
    echo "Examples:"
    echo "  $0 image:tag /tmp/data"
    echo "  INITIAL_SYNC_BLOCK=1100000 TARGET_SYNC_BLOCK=6100000 $0 image:tag /tmp/data"
    exit 1
fi

HIVED_IMAGE_NAME=$1
DATA_DIR=$2
shift 2

echo "Create environment..."
mkdir -vp "$DATA_DIR"
echo "Copy example config.ini to data dir"
cp "$CI_PROJECT_DIR/doc/example_config.ini" "$DATA_DIR/config.ini"

echo "Sapshot will be stored in: ${DATA_DIR}/snapshot"

HIVED_IMAGE_TAG="${HIVED_IMAGE_NAME##*:}"

# STEP 1: Initial sync to 1.1M (no pruning, split=9999 keeps all parts)
# Checkpoint ensures LIB reaches exactly block 1.1M with correct block_id, then node exits safely (also speeds up sync 2-3x)
"$SCRIPTS_PATH/build_and_setup_exchange_instance.sh" \
  $HIVED_IMAGE_TAG \
  --use-source-dir="$CI_PROJECT_DIR" \
  --data-dir="$DATA_DIR" \
  --exit-at-block="$INITIAL_SYNC_BLOCK" \
  --block-log-split=9999 \
  --name=sync-instance \
  --detach \
  --checkpoint="[\"${CHECKPOINT_BLOCK}\",\"${CHECKPOINT_BLOCK_ID}\"]" \
  --plugin=app_status_api \
  "$@"
monitor_container "sync-instance" 600  # 10 minutes timeout

# STEP 2: Dump snapshot at 1.1M (saves chainbase state to skip replay in STEP 3)
"$SCRIPTS_PATH/run_hived_img.sh" \
  "$HIVED_IMAGE_NAME" \
  --data-dir="$DATA_DIR" \
  --dump-snapshot="$SNAPSHOT_NAME" \
  --block-log-split=9999 \
  --exit-at-block="$INITIAL_SYNC_BLOCK" \
  --name=dump-snapshot-instance \
  --detach \
  --plugin=app_status_api
monitor_container "dump-snapshot-instance" 600  # 10 minutes timeout

# STEP 3: Load snapshot + sync 1.1M→5.1M with pruning (split=1 deletes parts 0001-0004, keeps 0005-0006)
echo "Starting load snapshot with pruned block log..."
"$SCRIPTS_PATH/run_hived_img.sh" \
  "$HIVED_IMAGE_NAME" \
  --data-dir="$DATA_DIR" \
  --load-snapshot="$SNAPSHOT_NAME" \
  --block-log-split=$BLOCK_LOG_SPLIT \
  --stop-at-block="$TARGET_SYNC_BLOCK" \
  --name=load-snapshot-instance \
  --detach \
  --plugin=app_status_api

# Phase 1: Wait for chain API to become ready
wait_for_chain_api_ready "load-snapshot-instance" 1200 || {
  echo "ERROR: Chain API did not become ready"
  stop_instance "load-snapshot-instance"
  exit 1
}

# Phase 2: Wait for sync to target block
echo "Waiting for load-snapshot-instance to reach block $TARGET_SYNC_BLOCK..."
HEAD_BLOCK=$(wait_for_container_block "load-snapshot-instance" $TARGET_SYNC_BLOCK "Sync progress" 120)

if [ -z "$HEAD_BLOCK" ]; then
  echo "ERROR: Node did not sync to block $TARGET_SYNC_BLOCK (timeout exceeded)"
  stop_instance "load-snapshot-instance"
  exit 1
fi

echo "Sync completed! Reached block $HEAD_BLOCK"

echo "Verifying API responses..."
# Test 1: get_dynamic_global_properties - verify head_block_number
echo "API returned head_block_number: $HEAD_BLOCK"

if [ "$HEAD_BLOCK" -ne "$TARGET_SYNC_BLOCK" ]; then
  echo "ERROR: Expected head_block_number $TARGET_SYNC_BLOCK (synced to stop-at-block), got $HEAD_BLOCK"
  stop_instance "load-snapshot-instance"
  exit 1
fi
echo "✓ head_block_number is $TARGET_SYNC_BLOCK (synced to stop-at-block limit)"

# Test 2: Verify test block is accessible (within non-pruned range)
echo "Verifying block $TEST_BLOCK is accessible (within kept part range)..."
BLOCK_RESPONSE=$(docker exec load-snapshot-instance wget -O - -q --timeout=30 --post-data="{\"jsonrpc\": \"2.0\",\"method\": \"block_api.get_block\",\"params\":{\"block_num\":$TEST_BLOCK},\"id\": 1}" localhost:8091)
if ! echo "$BLOCK_RESPONSE" | grep -q '"block_id"'; then
  echo "ERROR: Failed to get block $TEST_BLOCK via API"
  stop_instance "load-snapshot-instance"
  exit 1
fi
echo "✓ Block $TEST_BLOCK accessible via API"

echo "All block verification tests passed!"

# Stop the instance to ensure garbage collection completes
# The block log pruning uses lazy/asynchronous deletion - files are marked for
# deletion but cleaned up later when safe. Stopping the node forces completion
# of the garbage collection process before we verify the filesystem state.
echo "Stopping node to ensure garbage collection completes..."
stop_instance "load-snapshot-instance"

# Test 3: Verify pruned block log configuration (after node shutdown)
echo "Verifying pruned block log configuration (split=$BLOCK_LOG_SPLIT)..."

# Check which part files exist (directly on host filesystem, not via docker exec)
EXISTING_PARTS=$(ls "$DATA_DIR/blockchain/block_log_part".* 2>/dev/null | grep -o 'block_log_part\.[0-9]*' | sort | uniq)
echo "Existing block log parts: $EXISTING_PARTS"

# Verify pruned parts (should be deleted) - all parts before oldest_kept_part
echo "Checking pruned parts (should be deleted)..."
for ((part=1; part<OLDEST_KEPT_PART; part++)); do
  part_num=$(printf "%04d" $part)
  if [ -f "$DATA_DIR/blockchain/block_log_part.$part_num" ]; then
    echo "ERROR: block_log_part.$part_num should be deleted with split=$BLOCK_LOG_SPLIT at block $TARGET_SYNC_BLOCK"
    exit 1
  fi
  echo "  ✓ block_log_part.$part_num was properly pruned"
done

# Verify kept parts (should exist) - from oldest_kept_part to current_part
echo "Checking kept parts (should exist)..."
for ((part=OLDEST_KEPT_PART; part<=CURRENT_PART; part++)); do
  part_num=$(printf "%04d" $part)
  if [ ! -f "$DATA_DIR/blockchain/block_log_part.$part_num" ]; then
    echo "ERROR: block_log_part.$part_num should exist"
    exit 1
  fi
  echo "  ✓ block_log_part.$part_num exists"
done

echo "✓ Pruned block log verified: keeping split=$BLOCK_LOG_SPLIT part(s) from $(printf "%04d" $OLDEST_KEPT_PART) to $(printf "%04d" $CURRENT_PART)"
echo "✓ All tests passed!"
