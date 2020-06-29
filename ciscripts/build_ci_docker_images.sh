#! /bin/sh

REGISTRY=$1

docker build --target=runtime --build-arg CI_REGISTRY_IMAGE=$REGISTRY -t $REGISTRY/runtime -f Dockerfile.ci .
docker build --target=builder --build-arg CI_REGISTRY_IMAGE=$REGISTRY -t $REGISTRY/builder -f Dockerfile.ci .
docker build --target=test --build-arg CI_REGISTRY_IMAGE=$REGISTRY -t $REGISTRY/test -f Dockerfile.ci .
