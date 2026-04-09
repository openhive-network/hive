#!/bin/bash
set -euo pipefail

WORKDIR="/storage1/mario/mt-random-crash"
HIVED="/storage1/mario/00.src/hive/build_asan/programs/hived/hived"
BLOCK_LOG_DIR="/storage1/htm-mainnet/haf-datadir/blockchain"
DATADIR="${WORKDIR}/datadir"
LOGFILE="${WORKDIR}/asan-replay.log"
EXIT_BLOCK=25000000

echo "=== ASAN Replay to ${EXIT_BLOCK} blocks ==="
echo "Start time: $(date)"
echo "Hived: ${HIVED}"
echo "Block log: ${BLOCK_LOG_DIR}"
echo "Data dir: ${DATADIR}"
echo "Log file: ${LOGFILE}"

# Clean previous state
rm -rf "${DATADIR}"
mkdir -p "${DATADIR}/blockchain"

# Symlink block_log parts
for f in ${BLOCK_LOG_DIR}/block_log_part.*; do
    ln -s "$f" "${DATADIR}/blockchain/$(basename $f)"
done

echo "Block log parts linked: $(ls ${DATADIR}/blockchain/ | wc -l) files"

# ASAN options: halt on error, print full stack trace
export ASAN_OPTIONS="halt_on_error=1:print_stats=1:detect_leaks=0:log_path=${WORKDIR}/asan_errors"

echo "Starting replay..."
echo "Command: ${HIVED} --data-dir=${DATADIR} --replay-blockchain --exit-at-block ${EXIT_BLOCK} --shared-file-size 24G"

${HIVED} \
    --data-dir="${DATADIR}" \
    --replay-blockchain \
    --exit-at-block ${EXIT_BLOCK} \
    --shared-file-size 24G \
    --plugin=chain \
    --plugin=p2p \
    2>&1 | tee "${LOGFILE}"

EXIT_CODE=$?
echo ""
echo "=== Replay finished ==="
echo "Exit code: ${EXIT_CODE}"
echo "End time: $(date)"

# Check for ASAN error files
if ls ${WORKDIR}/asan_errors.* 1>/dev/null 2>&1; then
    echo "!!! ASAN ERRORS DETECTED !!!"
    cat ${WORKDIR}/asan_errors.*
fi
