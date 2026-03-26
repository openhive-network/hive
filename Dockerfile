# syntax=docker/dockerfile:1.6
# Base docker file having defined environment for build and run of hived instance.
# Build stage uses ci-base-image from common-ci-configuration (centralized build toolchain)
# To be started from cloned hive source directory.
ARG CI_REGISTRY_IMAGE=registry.gitlab.syncad.com/hive/hive/
ARG CI_IMAGE_TAG=ubuntu24.04-py3.14-1
ARG BUILD_IMAGE_TAG
ARG IMAGE_TAG_PREFIX
# CI base image for build stage - must be at top level for FROM to see it
# Using commit hash instead of tag to pin to a specific docker image for build repeatability.
# Tags can be moved/overwritten, but commit hashes are immutable. May switch to tags in future.
ARG CI_BASE_IMAGE=registry.gitlab.syncad.com/hive/common-ci-configuration/ci-base-image:fddc56669a22b53e6d75a6d697ee8a0f42a7ce52

FROM phusion/baseimage:noble-1.0.1 AS runtime

ENV LANG=en_US.UTF-8

SHELL ["/bin/bash", "-c"]

USER root
WORKDIR /usr/local/src
ADD ./scripts/setup_ubuntu.sh /usr/local/src/scripts/

# Install base runtime packages
RUN userdel --remove ubuntu
RUN ./scripts/setup_ubuntu.sh --runtime --hived-account="hived"

USER hived
WORKDIR /home/hived

FROM ubuntu:24.04 AS minimal-runtime

ENV LANG=en_US.UTF-8

SHELL ["/bin/bash", "-c"]

USER root
WORKDIR /usr/local/src
ADD ./scripts/openssl.conf ./scripts/setup_ubuntu.sh /usr/local/src/scripts/

# Install auto-apt-proxy for automatic apt caching proxy detection.
# On networks with an 'apt-proxy' DNS entry (our CI runners), apt traffic
# is routed through apt-cacher-ng. Silently falls back to direct on other networks.
RUN apt-get update && apt-get install -y --no-install-recommends auto-apt-proxy && rm -rf /var/lib/apt/lists/*

# Install base runtime packages
RUN userdel --remove ubuntu
RUN ./scripts/setup_ubuntu.sh --runtime --hived-account="hived"

USER hived
WORKDIR /home/hived

# Build stage uses centralized ci-base-image from common-ci-configuration
# This image includes: build toolchain, sccache, Pythons 3.8 - 3.14, glibc 2.28, Docker CLI, hived user
FROM ${CI_BASE_IMAGE} AS build

ARG BUILD_HIVE_TESTNET=OFF
ENV BUILD_HIVE_TESTNET=${BUILD_HIVE_TESTNET}

ARG HIVE_CONVERTER_BUILD=OFF
ENV HIVE_CONVERTER_BUILD=${HIVE_CONVERTER_BUILD}

ARG HIVE_LINT=OFF
ENV HIVE_LINT=${HIVE_LINT}

ARG HIVE_SUBDIR=.
ENV HIVE_SUBDIR=${HIVE_SUBDIR}

ARG SCCACHE_REDIS=""
ENV SCCACHE_REDIS=${SCCACHE_REDIS}

USER hived
WORKDIR /home/hived
SHELL ["/bin/bash", "-c"]

# Get everything from cwd as sources to be built.
COPY --chown=hived:users . /home/hived/source

RUN <<-EOF
  set -e

  # If SCCACHE_REDIS is empty, unset it so sccache uses local disk cache
  # instead of trying to connect to Redis with a malformed URL
  if [ -z "${SCCACHE_REDIS}" ]; then
    unset SCCACHE_REDIS
  fi

  INSTALLATION_DIR="/home/hived/bin"
  sudo mkdir -p "${INSTALLATION_DIR}"
  sudo chown hived:users "${INSTALLATION_DIR}"

  ./source/${HIVE_SUBDIR}/scripts/build.sh --source-dir="./source/${HIVE_SUBDIR}" --binary-dir="./build" \
  --cmake-arg="-DBUILD_HIVE_TESTNET=${BUILD_HIVE_TESTNET}" \
  --cmake-arg="-DHIVE_CONVERTER_BUILD=${HIVE_CONVERTER_BUILD}" \
  --cmake-arg="-DHIVE_LINT=${HIVE_LINT}" \
  --flat-binary-directory="${INSTALLATION_DIR}" \
  --clean-after-build

  # Show sccache statistics to verify distributed caching is working
  if command -v sccache &> /dev/null; then
    echo "=== sccache statistics ==="
    sccache --show-stats || true
  fi

  sudo chown -R hived:users "${INSTALLATION_DIR}/"*
EOF

FROM ${CI_REGISTRY_IMAGE}minimal-runtime:$CI_IMAGE_TAG AS instance

ARG BUILD_TIME
ARG GIT_COMMIT_SHA
ARG GIT_CURRENT_BRANCH
ARG GIT_LAST_LOG_MESSAGE
ARG GIT_LAST_COMMITTER
ARG GIT_LAST_COMMIT_DATE
LABEL org.opencontainers.image.created="$BUILD_TIME"
LABEL org.opencontainers.image.url="https://hive.io/"
LABEL org.opencontainers.image.documentation="https://gitlab.syncad.com/hive/hive"
LABEL org.opencontainers.image.source="https://gitlab.syncad.com/hive/hive"
#LABEL org.opencontainers.image.version="${VERSION}"
LABEL org.opencontainers.image.revision="$GIT_COMMIT_SHA"
LABEL org.opencontainers.image.licenses="MIT"
LABEL org.opencontainers.image.ref.name="HIVE Daemon"
LABEL org.opencontainers.image.title="Hive Daemon (hived) Image"
LABEL org.opencontainers.image.description="Runs hived instance. Contains various tools (including cli_wallet)"
LABEL io.hive.image.branch="$GIT_CURRENT_BRANCH"
LABEL io.hive.image.commit.log_message="$GIT_LAST_LOG_MESSAGE"
LABEL io.hive.image.commit.author="$GIT_LAST_COMMITTER"
LABEL io.hive.image.commit.date="$GIT_LAST_COMMIT_DATE"

ENV BUILD_IMAGE_TAG=${BUILD_IMAGE_TAG}

ARG P2P_PORT=2001
ENV P2P_PORT=${P2P_PORT}

ARG WS_PORT=8090
ENV WS_PORT=${WS_PORT}

ARG HTTP_PORT=8091
ENV HTTP_PORT=${HTTP_PORT}

ARG CLI_WALLET_PORT=8093
ENV CLI_WALLET_PORT=${CLI_WALLET_PORT}

ARG HIVE_SUBDIR=.
ENV HIVE_SUBDIR=${HIVE_SUBDIR}

SHELL ["/bin/bash", "-c"]

USER hived
WORKDIR /home/hived

RUN mkdir -p /home/hived/bin && \
    mkdir /home/hived/shm_dir && \
    mkdir /home/hived/datadir && \
    chown -Rc hived:users /home/hived/

COPY --from=build --chown=hived:users \
    /home/hived/bin/*  /home/hived/bin/

COPY --from=build --chown=hived:users /home/hived/source/${HIVE_SUBDIR}/doc/example_config.ini /home/hived/datadir/example_config.ini

COPY --from=build --chown=hived:users /home/hived/source/${HIVE_SUBDIR}/scripts /home/hived/scripts

COPY --chown=hived:users ./${HIVE_SUBDIR}/docker/docker_entrypoint.sh /home/hived/

VOLUME [ "/home/hived/datadir", "/home/hived/shm_dir" ]

ENV DATADIR=/home/hived/datadir
# Use default location (inside datadir) of shm file. If SHM should be placed on some different device, then set it to mapped volume `/home/hived/shm_dir` and map it in docker run
ENV SHM_DIR=${DATADIR}/blockchain

STOPSIGNAL SIGINT

# Expose HTTP port first (hardcoded for GitLab runner 18.6+ health check compatibility)
# Runner versions 18.6+ have issues detecting variable-based EXPOSE ports
EXPOSE 8091
#p2p service
EXPOSE ${P2P_PORT}
# websocket service
EXPOSE ${WS_PORT}
# Port specific to HTTP cli_wallet server
EXPOSE ${CLI_WALLET_PORT}

ENTRYPOINT [ "/home/hived/docker_entrypoint.sh" ]