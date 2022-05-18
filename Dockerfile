# Base docker file having defined environment for build and run of hived instance.
# Modify CI_IMAGE_TAG here and inside script hive/scripts/ci-helpers/build_ci_base_images.sh and run it. Then push images to registry
# To be started from cloned haf source directory.
ARG CI_REGISTRY_IMAGE=registry.gitlab.syncad.com/hive/hive/
ARG CI_IMAGE_TAG=:ubuntu20.04-4 
ARG BLOCK_LOG_SUFFIX

ARG BUILD_IMAGE_TAG

FROM phusion/baseimage:focal-1.0.0 AS runtime

ENV LANG=en_US.UTF-8

SHELL ["/bin/bash", "-c"] 

USER root
WORKDIR /usr/local/src
ADD ./scripts/setup_ubuntu.sh /usr/local/src/scripts/

# Install base runtime packages
RUN ./scripts/setup_ubuntu.sh --runtime --hived-account="hived"

USER hived
WORKDIR /home/hived

FROM ${CI_REGISTRY_IMAGE}runtime$CI_IMAGE_TAG AS ci-base-image

ENV LANG=en_US.UTF-8

SHELL ["/bin/bash", "-c"] 

USER root
WORKDIR /usr/local/src
# Install additionally development packages
RUN ./scripts/setup_ubuntu.sh --dev --hived-account="hived"

USER hived
WORKDIR /home/hived

#docker build --target=ci-base-image-5m -t registry.gitlab.syncad.com/hive/hive/ci-base-image-5m:ubuntu20.04-xxx -f Dockerfile .
FROM ${CI_REGISTRY_IMAGE}ci-base-image$CI_IMAGE_TAG AS ci-base-image-5m

RUN sudo -n mkdir -p /home/hived/datadir/blockchain && cd /home/hived/datadir/blockchain && \
  sudo -n wget -c https://gtg.openhive.network/get/blockchain/block_log.5M && \
    sudo -n mv block_log.5M block_log && sudo -n chown -Rc hived:hived /home/hived/datadir/

FROM ${CI_REGISTRY_IMAGE}ci-base-image$CI_IMAGE_TAG AS build

ARG BUILD_HIVE_TESTNET=OFF
ENV BUILD_HIVE_TESTNET=${BUILD_HIVE_TESTNET}

ARG HIVE_CONVERTER_BUILD=OFF
ENV HIVE_CONVERTER_BUILD=${HIVE_CONVERTER_BUILD}

ARG HIVE_LINT=ON
ENV HIVE_LINT=${HIVE_LINT}

USER hived
WORKDIR /home/hived
SHELL ["/bin/bash", "-c"] 

# Get everything from cwd as sources to be built.
COPY --chown=hived:hived . /home/hived/hive

RUN \
  ./hive/scripts/build.sh --source-dir="./hive" --binary-dir="./build" \
  --cmake-arg="-DBUILD_HIVE_TESTNET=${BUILD_HIVE_TESTNET}" \
  --cmake-arg="-DHIVE_CONVERTER_BUILD=${HIVE_CONVERTER_BUILD}" \
  --cmake-arg="-DHIVE_LINT=ON" \
  hived cli_wallet truncate_block_log && \
  cd ./build && \
  find . -name *.o  -type f -delete && \
  find . -name *.a  -type f -delete

# Here we could use a smaller image without packages specific to build requirements
FROM ${CI_REGISTRY_IMAGE}ci-base-image$BLOCK_LOG_SUFFIX$CI_IMAGE_TAG as base_instance

ENV BUILD_IMAGE_TAG=${BUILD_IMAGE_TAG:-:ubuntu20.04-4}

ARG P2P_PORT=2001
ENV P2P_PORT=${P2P_PORT}

ARG WS_PORT=8090
ENV WS_PORT=${WS_PORT}

ARG HTTP_PORT=8090
ENV HTTP_PORT=${HTTP_PORT}

SHELL ["/bin/bash", "-c"] 

USER hived
WORKDIR /home/hived

COPY --from=build /home/hived/build/programs/hived/hived /home/hived/build/programs/cli_wallet/cli_wallet /home/hived/build/programs/util/truncate_block_log /home/hived/bin/
COPY --from=build /home/hived/hive/scripts/common.sh ./scripts/common.sh

ADD ./docker/docker_entrypoint.sh .

RUN sudo -n mkdir -p /home/hived/bin && sudo -n mkdir -p /home/hived/shm_dir && \
  sudo -n mkdir -p /home/hived/datadir && sudo -n chown -Rc hived:hived /home/hived/

VOLUME [/home/hived/datadir, /home/hived/shm_dir]

STOPSIGNAL SIGINT 

ENTRYPOINT [ "/home/hived/docker_entrypoint.sh" ]

FROM ${CI_REGISTRY_IMAGE}base_instance$BLOCK_LOG_SUFFIX$BUILD_IMAGE_TAG as instance

#p2p service
EXPOSE ${P2P_PORT}
# websocket service
EXPOSE ${WS_PORT}
# JSON rpc service
EXPOSE ${HTTP_PORT}

FROM ${CI_REGISTRY_IMAGE}instance-5m$BUILD_IMAGE_TAG as data

ADD --chown=hived:hived ./docker/config_5M.ini /home/hived/datadir/config.ini

RUN "/home/hived/docker_entrypoint.sh" --force-replay --stop-replay-at-block=5000000 --exit-before-sync

ENTRYPOINT [ "/home/hived/docker_entrypoint.sh" ]

# default command line to be passed for this version (which should be stopped at 5M)
CMD ["--replay-blockchain", "--stop-replay-at-block=5000000"]

EXPOSE ${HTTP_PORT}

