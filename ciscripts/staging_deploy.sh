#!/bin/bash
HTTPS_PORT=$HTTPS_ENDPOINT_PORT
WS_PORT=$WS_ENDPOINT_PORT
ID=0
while (nc -z 127.0.0.1 $HTTPS_PORT && nc -z 127.0.0.1 $WS_PORT)
do
  ID=$(($ID +1))
  HTTPS_PORT=$(($HTTPS_PORT +2))
  WS_PORT=$(($WS_PORT +2))
done
echo $ID > ID.txt
screen -L -Logfile deploy.log -dmS hived-deploy-$CI_ENVIRONMENT_NAME-$ID ./hived --replay-blockchain --set-benchmark-interval 100000 --stop-replay-at-block $STOP_REPLAY_AT --webserver-http-endpoint 127.0.0.1:$HTTPS_PORT --webserver-ws-endpoint 127.0.0.1:$WS_PORT --advanced-benchmark --dump-memory-details  -d . 

if nc -z 127.0.0.1 $HTTPS_PORT && nc -z 127.0.0.1 $WS_PORT
 then
   echo "Hive server started successfully..."
   echo "webserver http endpoint: 127.0.0.1:$HTTPS_PORT"
   echo "webserver websocket endpoint: 127.0.0.1:$WS_PORT"
 else
   echo "Hive server start failure..."
fi