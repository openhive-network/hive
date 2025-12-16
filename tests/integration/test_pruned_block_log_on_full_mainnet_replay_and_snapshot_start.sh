#!/bin/bash

set -euo pipefail

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
SRC_DIR="$(realpath "${SCRIPTPATH}/../..")"

source "$SCRIPTPATH/conftest.sh"

if [ "$#" -lt 2 ]; then
    echo "Usage: $0 <hived_image_name> <data_dir> [block_log_source_dir]"
    echo ""
    echo "Arguments:"
    echo "  hived_image_name      Docker image name for hived"
    echo "  data_dir              Working directory for test data"
    echo "  block_log_source_dir  Directory containing block_log parts (optional, uses BLOCK_LOG_SOURCE_DIR env var if not provided)"
    echo ""
    echo "Environment variables:"
    echo "  LAST_PART             Override last part number to use (for testing with subset of data)"
    echo "                        Example: LAST_PART=5 uses only parts 0001-0005 even if more are available"
    echo ""
    echo "Examples:"
    echo "  $0 registry.gitlab.syncad.com/hive/hive:instance-1234 /tmp/test"
    echo "  LAST_PART=5 $0 image:tag /tmp/test  # Test with only 5M blocks"
    exit 1
fi

HIVED_IMAGE_NAME=$1
DATA_DIR=$2
BLOCK_LOG_SOURCE_DIR=${3:-${BLOCK_LOG_SOURCE_DIR:-}}

if [ -z "$BLOCK_LOG_SOURCE_DIR" ]; then
    echo "ERROR: Block log source directory not specified"
    echo "Either pass it as third argument or set BLOCK_LOG_SOURCE_DIR environment variable"
    exit 1
fi

if [ ! -d "$BLOCK_LOG_SOURCE_DIR" ]; then
    echo "ERROR: Block log source directory does not exist: $BLOCK_LOG_SOURCE_DIR"
    exit 1
fi

echo "=== Test Configuration ==="
echo "Hived image: $HIVED_IMAGE_NAME"
echo "Data directory: $DATA_DIR"
echo "Block log source: $BLOCK_LOG_SOURCE_DIR"
echo "=========================="

# Detect available block_log parts and calculate last block number
echo "Detecting block_log parts in $BLOCK_LOG_SOURCE_DIR..."

DETECTED_LAST_PART=$(get_last_part_number_from_dir "$BLOCK_LOG_SOURCE_DIR")
if [ $? -ne 0 ]; then
    echo "ERROR: Failed to detect block_log parts"
    exit 1
fi

# Allow override via environment variable for testing with subset of data
LAST_PART=${LAST_PART:-$DETECTED_LAST_PART}

if [ "$LAST_PART" -gt "$DETECTED_LAST_PART" ]; then
    echo "ERROR: LAST_PART ($LAST_PART) exceeds available parts ($DETECTED_LAST_PART)"
    exit 1
fi

echo "Detected parts: 0001-$(printf "%04d" $DETECTED_LAST_PART)"
if [ "$LAST_PART" -ne "$DETECTED_LAST_PART" ]; then
    echo "Using subset: 0001-$(printf "%04d" $LAST_PART) (LAST_PART override)"
fi

LAST_BLOCK_NUMBER=$((LAST_PART * 1000000))

# Get list of all parts for copying
BLOCK_LOG_PARTS=($(ls -1 "$BLOCK_LOG_SOURCE_DIR"/block_log_part.???? 2>/dev/null))

# Configuration
BLOCK_LOG_SPLIT=2                    # Keep 2 completed parts
BLOCKS_PER_PART=1000000             # 1M blocks per part

# Snapshot at LAST_BLOCK_NUMBER - 1M to allow sync from local data
SNAPSHOT_BLOCK=$((LAST_BLOCK_NUMBER - BLOCKS_PER_PART))
SNAPSHOT_PART=$(( SNAPSHOT_BLOCK / BLOCKS_PER_PART ))

# After sync to LAST_BLOCK_NUMBER:
#   - We'll copy 4 parts to test pruning with sync
SYNC_TARGET=$LAST_BLOCK_NUMBER
CURRENT_PART=$LAST_PART
OLDEST_KEPT_PART=$(( CURRENT_PART - BLOCK_LOG_SPLIT ))
PART_TO_COPY_START=$(( CURRENT_PART - 3 ))  # Copy 4 parts
TEST_BLOCK=$(( (OLDEST_KEPT_PART - 1) * BLOCKS_PER_PART + 100000 ))

echo "Found ${#BLOCK_LOG_PARTS[@]} block_log parts"
echo "Last part number: $LAST_PART ($(printf "%04d" $LAST_PART))"
echo "Target replay block: $LAST_BLOCK_NUMBER"
echo ""
echo "Pruning configuration:"
echo "  Snapshot block:     $SNAPSHOT_BLOCK (end of part $(printf "%04d" $SNAPSHOT_PART))"
echo "  Sync target:        $SYNC_TARGET (end of part $(printf "%04d" $CURRENT_PART))"
echo "  Block log split:    $BLOCK_LOG_SPLIT (keep $BLOCK_LOG_SPLIT completed parts)"
echo "  Current part:       $(printf "%04d" $CURRENT_PART)"
echo "  Oldest kept part:   $(printf "%04d" $OLDEST_KEPT_PART)"
echo "  Test block:         $TEST_BLOCK (in part $(printf "%04d" $((TEST_BLOCK / BLOCKS_PER_PART + 1))))"

# Setup directories
DATA_DIR_REPLAY_INSTANCE="$DATA_DIR/datadir-replay-instance"
DATA_DIR_SNAPSHOT_INSTANCE="$DATA_DIR/datadir-snapshot-instance"

mkdir -vp "${DATA_DIR_REPLAY_INSTANCE}/blockchain"

# Copy block_log parts to replay instance
echo "Copying block_log parts to replay instance..."
cp -v "${BLOCK_LOG_PARTS[@]}" "${BLOCK_LOG_PARTS[@]/%/.artifacts}" "$DATA_DIR_REPLAY_INSTANCE/blockchain/"

# STEP 1: Replay full mainnet data
echo "=== STEP 1: Replay mainnet to block $LAST_BLOCK_NUMBER (part $(printf "%04d" $LAST_PART)) ==="
"$SRC_DIR/scripts/run_hived_img.sh" \
    "$HIVED_IMAGE_NAME" \
    --data-dir="$DATA_DIR_REPLAY_INSTANCE" \
    --block-log-split=9999 \
    --exit-at-block=$LAST_BLOCK_NUMBER \
    --replay-blockchain \
    --shared-file-size=6G \
    --name=replay-instance \
    --detach \
    --plugin=app_status_api

monitor_container "replay-instance" 86400  # 24 hours timeout

# STEP 2: Dump snapshot at SNAPSHOT_BLOCK (1M before end)
echo "=== STEP 2: Dump snapshot at block $SNAPSHOT_BLOCK (part $(printf "%04d" $SNAPSHOT_PART)) ==="
SNAPSHOT_NAME="snapshot-$SNAPSHOT_BLOCK"

"$SRC_DIR/scripts/run_hived_img.sh" \
    "$HIVED_IMAGE_NAME" \
    --data-dir="$DATA_DIR_REPLAY_INSTANCE" \
    --block-log-split=9999 \
    --dump-snapshot="$SNAPSHOT_NAME" \
    --exit-at-block=$SNAPSHOT_BLOCK \
    --shared-file-size=6G \
    --name=dump-snapshot-instance \
    --detach \
    --plugin=app_status_api

monitor_container "dump-snapshot-instance" 7200  # 2 hours timeout

# STEP 3: Prepare environment for loading snapshot with pruned block log (split=2)
echo "=== STEP 3: Prepare snapshot instance with pruned block log ==="
mkdir -vp "${DATA_DIR_SNAPSHOT_INSTANCE}/blockchain"

# Copy snapshot
echo "Copying snapshot..."
cp -rv "$DATA_DIR_REPLAY_INSTANCE/snapshot/" "$DATA_DIR_SNAPSHOT_INSTANCE/" || {
    echo "ERROR: Failed to copy snapshot"
    exit 1
}

# Copy last 4 block_log parts (to test pruning with sync)
# After sync: CURRENT_PART will be active, pruning with split=2
echo "Copying last 4 block_log parts for pruning test..."
LAST_FOUR_PARTS=( $(ls -1 "${DATA_DIR_REPLAY_INSTANCE}/blockchain"/block_log_part.???? | tail -4) )

if [ ${#LAST_FOUR_PARTS[@]} -lt 4 ]; then
    echo "ERROR: Need at least 4 block_log parts for pruning test"
    exit 1
fi

echo "Copying parts: ${LAST_FOUR_PARTS[@]}"
cp -v "${LAST_FOUR_PARTS[@]}" "${LAST_FOUR_PARTS[@]/%/.artifacts}" "$DATA_DIR_SNAPSHOT_INSTANCE/blockchain/"

# Display which parts will be kept/pruned after sync
PART_TO_PRUNE=$(( CURRENT_PART - 3 ))
echo ""
echo "Expected pruning behavior after sync:"
echo "  Part $(printf "%04d" $PART_TO_PRUNE) - ❌ WILL BE PRUNED (too old)"
echo "  Parts $(printf "%04d" $OLDEST_KEPT_PART)-$(printf "%04d" $CURRENT_PART) - ✅ WILL BE KEPT (split=$BLOCK_LOG_SPLIT)"

# Load snapshot with split=2
echo "Loading snapshot with block-log-split=$BLOCK_LOG_SPLIT..."
"$SRC_DIR/scripts/run_hived_img.sh" \
    "$HIVED_IMAGE_NAME" \
    --data-dir="$DATA_DIR_SNAPSHOT_INSTANCE" \
    --block-log-split=$BLOCK_LOG_SPLIT \
    --load-snapshot="$SNAPSHOT_NAME" \
    --exit-before-sync \
    --shared-file-size=6G \
    --name=load-snapshot-instance \
    --detach \
    --plugin=app_status_api

monitor_container "load-snapshot-instance" 3600  # 1 hour timeout

# STEP 4: Sync from SNAPSHOT_BLOCK to SYNC_TARGET with pruning
echo "=== STEP 4: Sync from $SNAPSHOT_BLOCK to $SYNC_TARGET with split=$BLOCK_LOG_SPLIT ==="

"$SRC_DIR/scripts/run_hived_img.sh" \
    "$HIVED_IMAGE_NAME" \
    --data-dir="$DATA_DIR_SNAPSHOT_INSTANCE" \
    --block-log-split=$BLOCK_LOG_SPLIT \
    --stop-at-block=$SYNC_TARGET \
    --shared-file-size=6G \
    --name=testing-instance \
    --detach \
    --plugin=app_status_api

# Wait for chain API to become ready
wait_for_chain_api_ready "testing-instance" 900 || {
    echo "ERROR: Chain API did not become ready"
    stop_instance "testing-instance"
    exit 1
}

# Wait for sync to complete
HEAD_BLOCK=$(wait_for_container_block "testing-instance" $SYNC_TARGET "Sync progress" 3600)

if [ -z "$HEAD_BLOCK" ]; then
  echo "ERROR: Node did not sync to block $SYNC_TARGET (timeout exceeded)"
  stop_instance "testing-instance"
  exit 1
fi

echo "Sync completed! Reached block $HEAD_BLOCK"

echo "Verifying API responses..."
# Test 1: get_dynamic_global_properties - verify head_block_number
echo "API returned head_block_number: $HEAD_BLOCK"

if [ "$HEAD_BLOCK" -ne "$SYNC_TARGET" ]; then
  echo "ERROR: Expected head_block_number $SYNC_TARGET (synced to stop-at-block), got $HEAD_BLOCK"
  stop_instance "testing-instance"
  exit 1
fi
echo "✓ head_block_number is $SYNC_TARGET (synced to stop-at-block limit)"

# Test 2: Verify test block is accessible (within non-pruned range)
echo "Verifying block $TEST_BLOCK is accessible (within kept part range)..."
BLOCK_RESPONSE=$(docker exec testing-instance wget -O - -q --timeout=30 --post-data="{\"jsonrpc\": \"2.0\",\"method\": \"block_api.get_block\",\"params\":{\"block_num\":$TEST_BLOCK},\"id\": 1}" localhost:8091)
if ! echo "$BLOCK_RESPONSE" | grep -q '"block_id"'; then
  echo "ERROR: Failed to get block $TEST_BLOCK via API"
  stop_instance "testing-instance"
  exit 1
fi
echo "✓ Block $TEST_BLOCK accessible via API"

echo "All block verification tests passed!"

# Stop the instance to ensure garbage collection completes
# The block log pruning uses lazy/asynchronous deletion - files are marked for
# deletion but cleaned up later when safe. Stopping the node forces completion
# of the garbage collection process before we verify the filesystem state.
echo "Stopping node to ensure garbage collection completes..."
stop_instance "testing-instance"

# Test 3: Verify pruned block log configuration (after node shutdown)
echo "Verifying pruned block log configuration (split=$BLOCK_LOG_SPLIT)..."

# Check which part files exist (directly on host filesystem)
EXISTING_PARTS=$(ls "$DATA_DIR_SNAPSHOT_INSTANCE/blockchain/block_log_part".* 2>/dev/null | grep -o 'block_log_part\.[0-9]*' | sort | uniq)
echo "Existing block log parts: $EXISTING_PARTS"

# Verify pruned parts (should be deleted)
echo "Checking pruned parts (should be deleted)..."
PART_TO_PRUNE=$(( CURRENT_PART - 3 ))
part_num=$(printf "%04d" $PART_TO_PRUNE)
if [ -f "$DATA_DIR_SNAPSHOT_INSTANCE/blockchain/block_log_part.$part_num" ]; then
  echo "ERROR: block_log_part.$part_num should be deleted with split=$BLOCK_LOG_SPLIT"
  exit 1
fi
echo "  ✓ block_log_part.$part_num was properly pruned"

# Verify kept parts (should exist) - from oldest_kept_part to current_part
echo "Checking kept parts (should exist)..."
for ((part=OLDEST_KEPT_PART; part<=CURRENT_PART; part++)); do
  part_num=$(printf "%04d" $part)
  if [ ! -f "$DATA_DIR_SNAPSHOT_INSTANCE/blockchain/block_log_part.$part_num" ]; then
    echo "ERROR: block_log_part.$part_num should exist"
    exit 1
  fi
  echo "  ✓ block_log_part.$part_num exists"
done

echo "✓ Pruned block log verified: keeping split=$BLOCK_LOG_SPLIT part(s) from $(printf "%04d" $OLDEST_KEPT_PART) to $(printf "%04d" $CURRENT_PART)"
echo "✓ All tests passed!"
