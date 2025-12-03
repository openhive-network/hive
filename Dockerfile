# syntax=docker/dockerfile:1.6
# Base docker file having defined environment for build and run of hived instance.
# Modify CI_IMAGE_TAG here and inside script hive/scripts/ci-helpers/build_ci_base_image.sh and run it. Then push images to registry
# To be started from cloned haf source directory.
ARG CI_REGISTRY_IMAGE=registry.gitlab.syncad.com/hive/hive/
ARG CI_IMAGE_TAG=ubuntu24.04-3
ARG BUILD_IMAGE_TAG
ARG IMAGE_TAG_PREFIX

FROM phusion/baseimage:noble-1.0.1 AS runtime

ENV LANG=en_US.UTF-8

SHELL ["/bin/bash", "-c"]

USER root
WORKDIR /usr/local/src
ADD ./scripts/setup_ubuntu.sh /usr/local/src/scripts/

# Install base runtime packages
RUN userdel --remove ubuntu
RUN ./scripts/setup_ubuntu.sh --runtime --hived-admin-account="hived_admin" --hived-account="hived"

USER hived_admin
WORKDIR /home/hived_admin

FROM ubuntu:24.04 AS minimal-runtime

ENV LANG=en_US.UTF-8

SHELL ["/bin/bash", "-c"]

USER root
WORKDIR /usr/local/src
ADD ./scripts/openssl.conf ./scripts/setup_ubuntu.sh /usr/local/src/scripts/

# Install base runtime packages
RUN userdel --remove ubuntu
RUN ./scripts/setup_ubuntu.sh --runtime --hived-admin-account="hived_admin" --hived-account="hived"

USER hived_admin
WORKDIR /home/hived_admin

FROM ${CI_REGISTRY_IMAGE}runtime:$CI_IMAGE_TAG AS ci-base-image

ENV LANG=en_US.UTF-8
ENV PATH="/home/hived_admin/.local/bin:$PATH"

SHELL ["/bin/bash", "-c"]

USER root
WORKDIR /usr/local/src

# Install additionally development packages
RUN ./scripts/setup_ubuntu.sh --dev --hived-admin-account="hived_admin" --hived-account="hived"

USER hived_admin
WORKDIR /home/hived_admin

# Install additionally packages located in user directory
RUN /usr/local/src/scripts/setup_ubuntu.sh --user

# Install Docker CLI
RUN /usr/local/src/scripts/setup_ubuntu.sh --docker-cli=28.0.1

FROM ${CI_REGISTRY_IMAGE}ci-base-image:$CI_IMAGE_TAG AS build

ARG BUILD_HIVE_TESTNET=OFF
ENV BUILD_HIVE_TESTNET=${BUILD_HIVE_TESTNET}

ARG ENABLE_SMT_SUPPORT=OFF
ENV ENABLE_SMT_SUPPORT=${ENABLE_SMT_SUPPORT}

ARG HIVE_CONVERTER_BUILD=OFF
ENV HIVE_CONVERTER_BUILD=${HIVE_CONVERTER_BUILD}

ARG HIVE_LINT=OFF
ENV HIVE_LINT=${HIVE_LINT}

ARG HIVE_SUBDIR=.
ENV HIVE_SUBDIR=${HIVE_SUBDIR}

USER hived_admin
WORKDIR /home/hived_admin
SHELL ["/bin/bash", "-c"]

# Get everything from cwd as sources to be built.
COPY --chown=hived_admin:users . /home/hived_admin/source

RUN <<-EOF
  set -e

  INSTALLATION_DIR="/home/hived/bin"
  sudo --user=hived mkdir -p "${INSTALLATION_DIR}"

  ./source/${HIVE_SUBDIR}/scripts/build.sh --source-dir="./source/${HIVE_SUBDIR}" --binary-dir="./build" \
  --cmake-arg="-DBUILD_HIVE_TESTNET=${BUILD_HIVE_TESTNET}" \
  --cmake-arg="-DENABLE_SMT_SUPPORT=${ENABLE_SMT_SUPPORT}" \
  --cmake-arg="-DHIVE_CONVERTER_BUILD=${HIVE_CONVERTER_BUILD}" \
  --cmake-arg="-DHIVE_LINT=${HIVE_LINT}" \
  --flat-binary-directory="${INSTALLATION_DIR}" \
  --clean-after-build

  sudo chown -R hived "${INSTALLATION_DIR}/"*
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

COPY --from=build --chown=hived:users /home/hived_admin/source/${HIVE_SUBDIR}/doc/example_config.ini /home/hived/datadir/example_config.ini

USER hived_admin
WORKDIR /home/hived_admin

COPY --from=build --chown=hived_admin:users /home/hived_admin/source/${HIVE_SUBDIR}/scripts ./scripts

COPY --chown=hived_admin:users ./${HIVE_SUBDIR}/docker/docker_entrypoint.sh .

VOLUME [ "/home/hived/datadir", "/home/hived/shm_dir" ]

# Always define default value of HIVED_UID variable to make possible direct spawn of docker image (without run_hived_img.sh wrapper)
ENV HIVED_UID=1000
ENV DATADIR=/home/hived/datadir
# Use default location (inside datadir) of shm file. If SHM should be placed on some different device, then set it to mapped volume `/home/hived/shm_dir` and map it in docker run
ENV SHM_DIR=${DATADIR}/blockchain

STOPSIGNAL SIGINT

#p2p service
EXPOSE ${P2P_PORT}
# websocket service
EXPOSE ${WS_PORT}
# JSON rpc service
EXPOSE ${HTTP_PORT}
# Port specific to HTTP cli_wallet server
EXPOSE ${CLI_WALLET_PORT}

ENTRYPOINT [ "/home/hived_admin/docker_entrypoint.sh" ]