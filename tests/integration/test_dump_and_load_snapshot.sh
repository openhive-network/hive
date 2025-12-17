#! /bin/bash -x

set -xeuo pipefail

rm -rf ./datadir
mkdir -vp "./datadir/blockchain"
cp /workspace/block_logs/mainnet_5M/block_log ./datadir/blockchain/block_log

/workspace/hive/build_mainnet/programs/hived/hived \
    --data-dir="/workspace/hive/tests/integration/datadir" \
    --dump-snapshot=1m \
    --replay \
    --stop-at-block=1100000 \
    --exit-before-sync 2>&1 | tee hived_dump_snapshot.log

echo "Set block-log-split option to 0, and no storage blocks"
/workspace/hive/build_mainnet/programs/hived/hived \
    --data-dir="/workspace/hive/tests/integration/datadir" \
    --replay \
    --load-snapshot=1m \
    --exit-before-sync \
    --block-log-split=0 2>&1 | tee hived_load_snapshot_no_storage_block_log_parts.log

echo "Set block-log-split option to 2, and storaga last 2 milion blocks"
/workspace/hive/build_mainnet/programs/hived/hived \
    --data-dir="/workspace/hive/tests/integration/datadir" \
    --load-snapshot=1m \
    --replay \
    --block-log-split=2 2>&1 | tee hived_load_snapshot_storage_block_log_parts.log


# najpierw split na 0 a potem już na 2 i ma gromadzić bloki
# pierwotne
#./hived --replay --stop-at-block=101060000 --dump-snapshot=101m --exit-before-sync --data-dir="$(pwd)"
#./bin/hived --replay --data-dir=./datadir --load-snapshot=101m --block-log-split=0 --exit-before-sync

# zrobić skrypt który włącza run_hived_instance z możliwością przekazania opcji do hived i domyślinie włączoną opcją --dump-snapshot i ustawionym splitem
