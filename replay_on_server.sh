#!/bin/bash
# Replay script for reproducing pay_curators crash on remote server
# Run inside: /workspace/mario/data_copy/mt-random-crash/
# Usage: screen -S mario-replay then ./replay_on_server.sh

set -e

DATADIR="/workspace/disk_nvme"
ORIGINAL_HIVED="/workspace/mario/data_copy/mt-random-crash/original/hived"
ASAN_HIVED="/workspace/mario/data_copy/mt-random-crash/asan/hived"
LOGDIR="/workspace/mario/data_copy/mt-random-crash"

echo "============================================"
echo "Phase 1: Original hived replay 0 -> 24M blocks"
echo "============================================"

# Clean up any existing shared_memory.bin to start fresh
rm -f "${DATADIR}/shared_memory.bin"

time "${ORIGINAL_HIVED}" \
    -d "${DATADIR}" \
    --force-replay \
    --exit-at-block 24000000 \
    --set-benchmark-interval 100000 \
    2>&1 | tee -i "${LOGDIR}/mario-phase1.log"

echo ""
echo "Phase 1 complete. shared_memory.bin state at block 24M."
echo ""

echo "============================================"
echo "Phase 2: ASAN hived replay 24M -> 25M blocks"
echo "  (with tree validation + ASAN, no memory dump)"
echo "============================================"

# Phase 2: continue from where phase 1 left off (no --force-replay)
# ASAN_OPTIONS: halt_on_error=1 to stop immediately on detection
export ASAN_OPTIONS="halt_on_error=1:detect_stack_use_after_return=1:check_initialization_order=1:strict_init_order=1:detect_leaks=0"

time "${ASAN_HIVED}" \
    -d "${DATADIR}" \
    --replay-blockchain \
    --exit-at-block 25000000 \
    --set-benchmark-interval 100000 \
    2>&1 | tee -i "${LOGDIR}/mario-phase2.log"

echo ""
echo "Phase 2 complete."
echo "Check mario-phase2.log for ASAN errors or tree validation failures."
