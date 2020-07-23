#!/bin/bash
$CI_PROJECT_DIR/$CI_ENVIRONMENT_NAME/deploy/hived --replay-blockchain --set-benchmark-interval 100000 --stop-replay-at-block ${STOP_DEPLOY_AT} --advanced-benchmark --dump-memory-details  -d $CI_PROJECT_DIR/$CI_ENVIRONMENT_NAME/deploy --shared-file-dir $CI_PROJECT_DIR/$CI_ENVIRONMENT_NAME/deploy 2>&1 | tee $CI_PROJECT_DIR/$CI_ENVIRONMENT_NAME/deploy/replay.log
