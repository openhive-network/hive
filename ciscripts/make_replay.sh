#!/bin/bash
set -e

CI_COMMIT_SHORT_SHA=$1
STOP_REPLAY_AT_BLOCK=$2
CONTAINER=`docker ps | grep "consensus_node.${CI_COMMIT_SHORT_SHA}.replay" | awk '{print $1}'`
FREE_SPACE=`df . | tail -1 | awk {'print $4'}`

if ([ $STOP_REPLAY_AT_BLOCK -gt 5000000  ] && [ $FREE_SPACE -le 400000000 ]) || ([ $STOP_REPLAY_AT_BLOCK -eq 5000000  ] && [ $FREE_SPACE -le 80000000 ]); then
  echo  "`date`: ERROR: no free space available, please clear old deployed instances!"
  exit 1
fi

if [ $STOP_REPLAY_AT_BLOCK -le 5000000 ] ;then
    COPY_DATA="data/test/block_log"
else
    COPY_DATA="data/full/block_log"
fi

if [[ -n $CONTAINER ]] ; then
    echo "`date`: ERROR: The replay container is currently running: consensus_node${CONTAINER}. Please wait for replay to finish and try again" | tee -a ${CI_COMMIT_SHORT_SHA}.replay.log
    exit 1
else
    rm -fr ${CI_COMMIT_SHORT_SHA}.replay
    rm -f ${CI_COMMIT_SHORT_SHA}.replay.log
    mkdir -p {./${CI_COMMIT_SHORT_SHA}.replay,./${CI_COMMIT_SHORT_SHA}.replay/blockchain}
    mv DEPLOY_CONFIG_INI ./${CI_COMMIT_SHORT_SHA}.replay/config.ini
    cp ${COPY_DATA} ${CI_COMMIT_SHORT_SHA}.replay/blockchain/
    echo "`date`: INFO: directory ${CI_COMMIT_SHORT_SHA}.replay created" | tee -a ${CI_COMMIT_SHORT_SHA}.replay.log
    echo "`date`: INFO: start replay at block: ${STOP_REPLAY_AT_BLOCK}" | tee -a ${CI_COMMIT_SHORT_SHA}.replay.log
    TEST_PARAM='cd /usr/local/hive/consensus/datadir ; /usr/local/hive/consensus/bin/hived --webserver-ws-endpoint=0.0.0.0:8090 --webserver-http-endpoint=0.0.0.0:8090 --p2p-endpoint=0.0.0.0:2001 --exit-after-replay --replay-blockchain --set-benchmark-interval 100000 --stop-replay-at-block '
    TEST_PARAM+=$STOP_REPLAY_AT_BLOCK
    TEST_PARAM+=' --advanced-benchmark --dump-memory-details -d /usr/local/hive/consensus/datadir'
    echo $CI_REGISTRY_PASSWORD | docker login -u $CI_REGISTRY_USER --password-stdin $CI_REGISTRY
    docker container run -d -ti --user `id -u`:`id -g` --name=consensus_node.${CI_COMMIT_SHORT_SHA}.replay -v ~/${CI_COMMIT_SHORT_SHA}.replay:/usr/local/hive/consensus/datadir $CI_REGISTRY_IMAGE/consensus_node:$CI_COMMIT_SHORT_SHA bash
    docker logout $CI_REGISTRY
    start_time=$SECONDS
    docker container exec -t consensus_node.${CI_COMMIT_SHORT_SHA}.replay bash -c "${TEST_PARAM}" 2>&1 | tee -i ${CI_COMMIT_SHORT_SHA}.replay/blockchain/replay.log
    elapsed=$(( SECONDS - start_time ))
    docker container stop consensus_node.${CI_COMMIT_SHORT_SHA}.replay
    docker container rm consensus_node.${CI_COMMIT_SHORT_SHA}.replay
    tar -czf ./${CI_COMMIT_SHORT_SHA}.replay/stats.tgz ./${CI_COMMIT_SHORT_SHA}.replay/*.json
    tar -czf ./${CI_COMMIT_SHORT_SHA}.replay/replay.log.tgz ./${CI_COMMIT_SHORT_SHA}.replay/blockchain/replay.log
    echo "`date`: INFO: block_log replay completed" >> ${CI_COMMIT_SHORT_SHA}.replay.log
    eval "echo Elapsed time: $(date -ud "@$elapsed" +'$((%s/3600/24)) days %H hr %M min %S sec')" >> ${CI_COMMIT_SHORT_SHA}.replay.log
    docker rmi `docker image ls -a | grep consensus_node.*${CI_COMMIT_SHORT_SHA} | awk '{print $3}'`
fi
