# #!/bin/bash
set -e

START_STOP=$1
WS_PORT=$2
P2P_PORT=$(( 2001+WS_PORT-8090 ))
CI_COMMIT_SHORT_SHA=$3
CONTAINER=`docker ps | grep "consensus_node.deploy_${WS_PORT}.hived" | awk '{print $1}'`
TEST_RUN_PARAM='/usr/local/hive/consensus/bin/hived --webserver-ws-endpoint=0.0.0.0:8090 --webserver-http-endpoint=0.0.0.0:8090 --p2p-endpoint=0.0.0.0:2001 --replay-blockchain --data-dir=/usr/local/hive/consensus/datadir  2>&1 | tee -a datadir/blockchain/replay.log'
FREE_SPACE=`df . | tail -1 | awk {'print $4'}`

if ([ $STOP_REPLAY_AT_BLOCK -gt 5000000  ] && [ $FREE_SPACE -le 400000000 ]) || ([ $STOP_REPLAY_AT_BLOCK -eq 5000000  ] && [ $FREE_SPACE -le 80000000 ]); then
  echo  "`date`: ERROR: No free space available, please clear old deployed instances!"
  exit 1
fi

case $START_STOP in
  STOP)
    if [[ -n $CONTAINER ]] ; then
      docker container stop $CONTAINER || true
      docker container rm $CONTAINER || true
    fi
    rm -fr  deploy_${WS_PORT}_dev_dir
    rm deploy_${WS_PORT}_dev.log
  ;;

  START)
    if [[ -n $CONTAINER ]] ; then
      docker container stop $CONTAINER || true
      docker container rm $CONTAINER || true
    fi

    rm -fr  deploy_${WS_PORT}_dev_dir || true
    mkdir -p deploy_${WS_PORT}_dev_dir || true

    if [ -d ${CI_COMMIT_SHORT_SHA}.replay ] ; then
      cp -r ${CI_COMMIT_SHORT_SHA}.replay/* deploy_${WS_PORT}_dev_dir
      rm -f deploy_${WS_PORT}_dev_dir/blockchain/replay.log
    else
      echo "`date`: ERROR: ${CI_COMMIT_SHORT_SHA}.replay folder doesn't exist! Make replay and try again"
      exit 1
    fi

    mv DEPLOY_CONFIG_INI ./deploy_${WS_PORT}_dev_dir/config.ini
    echo $CI_REGISTRY_PASSWORD | docker login -u $CI_REGISTRY_USER --password-stdin $CI_REGISTRY
    docker container run -d -ti -p $P2P_PORT:2001 -p $WS_PORT:8090 --user `id -u`:`id -g` --name consensus_node.deploy_${WS_PORT}.hived -v ~/deploy_${WS_PORT}_dev_dir:/usr/local/hive/consensus/datadir --restart always $CI_REGISTRY_IMAGE/consensus_node:$CI_COMMIT_SHORT_SHA bash
    docker logout $CI_REGISTRY
    nohup docker exec -t consensus_node.deploy_${WS_PORT}.hived bash -c "${TEST_RUN_PARAM}" > /dev/null 2>&1 &
    echo "`date`: SUCCESS: container consensus_node.deploy_${WS_PORT}.hived ( ${CI_COMMIT_SHORT_SHA} ) started at ports WS_PORT: ${WS_PORT} P2P_PORT: ${P2P_PORT}" | tee -a deploy_${WS_PORT}_dev.log
  ;;
esac
