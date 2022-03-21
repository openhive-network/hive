# Base docker file having defined environment for build and run of HAF instance.
# docker build --target=ci-base-image -t registry.gitlab.syncad.com/hive/hive/ci-base-image:ubuntu20.04-xxx -f Dockerfile .
# To be started from cloned haf source directory.
ARG CI_REGISTRY_IMAGE=registry.gitlab.syncad.com/hive/hive
ARG CI_IMAGE_TAG=:ubuntu20.04-4 
ARG BLOCK_LOG_SUFFIX=-5m

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

FROM $CI_REGISTRY_IMAGE/runtime$CI_IMAGE_TAG AS ci-base-image

ENV LANG=en_US.UTF-8

SHELL ["/bin/bash", "-c"] 

USER root
WORKDIR /usr/local/src
# Install additionally development packages
RUN ./scripts/setup_ubuntu.sh --dev --hived-account="hived"

USER hived
WORKDIR /home/hived

#docker build --target=ci-base-image-5m -t registry.gitlab.syncad.com/hive/hive/ci-base-image-5m:ubuntu20.04-xxx -f Dockerfile .
FROM $CI_REGISTRY_IMAGE/ci-base-image$CI_IMAGE_TAG AS ci-base-image-5m

RUN sudo -n mkdir -p /home/hived/datadir/blockchain && cd /home/hived/datadir/blockchain && \
  sudo -n wget -c https://gtg.openhive.network/get/blockchain/block_log.5M && \
    sudo -n mv block_log.5M block_log && sudo -n chown -Rc hived:hived /home/hived/datadir/
