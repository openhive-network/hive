#!/bin/bash
screen -L -Logfile deploy.log -dmS hived-deploy-$CI_ENVIRONMENT_NAME $CI_PROJECT_DIR/sync/$CI_ENVIRONMENT_NAME/hived --replay-blockchain --set-benchmark-interval 100000 --stop-replay-at-block ${STOP_REPLAY_AT} --advanced-benchmark --dump-memory-details  -d $CI_PROJECT_DIR/sync/$CI_ENVIRONMENT_NAME --shared-file-dir $CI_PROJECT_DIR/sync/$CI_ENVIRONMENT_NAME
