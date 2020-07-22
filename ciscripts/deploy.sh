#!/bin/bash
$CI_PROJECT_DIR/deploy/hived --replay-blockchain --set-benchmark-interval 100000 --stop-replay-at-block ${STOP_DEPLOY_AT} --advanced-benchmark --dump-memory-details  -d $CI_PROJECT_DIR/deploy --shared-file-dir $CI_PROJECT_DIR/deploy 2>&1 | tee $CI_PROJECT_DIR/deploy/replay.log
