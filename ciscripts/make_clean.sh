#!/bin/bash
set -e
CI_COMMIT_SHORT_SHA=$1

CONTAINERS=`docker ps | grep consensus_node.*${CI_COMMIT_SHORT_SHA} | awk '{print $1}'`
IMAGE=`docker image ls -a | grep consensus_node.*${CI_COMMIT_SHORT_SHA} | awk '{print $3}'`

for CONTAINER in "${CONTAINERS[@]}" ; do
    if [[ -n $CONTAINER ]] ; then
        echo "`date`: INFO: stop and clean the container after working ${CONTAINER}."
        docker container stop $CONTAINER || true
        docker container rm $CONTAINER || true
        echo "`date`: INFO: +done"
    else
        echo "`date`: INFO: no running container"
    fi
done

if [[ -n $IMAGE ]] ; then
    echo "`date`: delete  image ${IMAGE}"
    docker rmi $IMAGE || true
    echo "`date`: INFO: +done"
else
    echo "`date`: INFO: no image ${IMAGE}"
fi

if [[ -d $CI_COMMIT_SHORT_SHA.replay ]]  ; then
        rm -fr $CI_COMMIT_SHORT_SHA.replay || true
        rm -f $CI_COMMIT_SHORT_SHA.replay.log || true
fi
