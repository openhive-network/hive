#! /bin/bash

set -euo pipefail 

REGISTRY=$1

docker build --target=runtime --build-arg CI_REGISTRY_IMAGE=$REGISTRY -t $REGISTRY/runtime:ubuntu-20.04 -f Dockerfile.ci .
docker build --target=builder --build-arg CI_REGISTRY_IMAGE=$REGISTRY -t $REGISTRY/builder:ubuntu-20.04 -f Dockerfile.ci .
docker build --target=test --build-arg CI_REGISTRY_IMAGE=$REGISTRY -t $REGISTRY/test:ubuntu-20.04 -f Dockerfile.ci .
