#!/bin/bash
HTTPS_PORT=${HTTPS_ENDPOINT_PORT}
WS_PORT=${WS_ENDPOINT_PORT}
if nc -z 127.0.0.1 $HTTPS_PORT && nc -z 127.0.0.1 $WS_PORT
  then
    screen -S hived-deploy-$CI_ENVIRONMENT_NAME -X quit
fi
    
nohup $CI_PROJECT_DIR/replay/$CI_ENVIRONMENT_NAME/hived --replay-blockchain --set-benchmark-interval 100000 --stop-replay-at-block $STOP_REPLAY_AT --webserver-http-endpoint 127.0.0.1:$HTTPS_PORT --webserver-ws-endpoint 127.0.0.1:$WS_PORT --advanced-benchmark --dump-memory-details  -d $CI_PROJECT_DIR/replay/$CI_ENVIRONMENT_NAME --shared-file-dir $CI_PROJECT_DIR/replay/$CI_ENVIRONMENT_NAME > deploy.log 2>&1 &
echo $! > pid.txt

if nc -z 127.0.0.1 $HTTPS_PORT && nc -z 127.0.0.1 $WS_PORT
  then
    echo "Hive server started successfully..."
    echo "Hive server http endpoint: 127.0.0.1:$HTTPS_PORT"
    echo "Hive server websocket endpoint: 127.0.0.1:$WS_PORT"
  else
    echo "Hive server start failure..."
fi