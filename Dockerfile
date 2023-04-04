# Base docker file having defined environment for build and run of hived instance.
# Modify CI_IMAGE_TAG here and inside script hive/scripts/ci-helpers/build_ci_base_images.sh and run it. Then push images to registry
# To be started from cloned haf source directory.
ARG CI_REGISTRY_IMAGE=registry.gitlab.syncad.com/hive/hive/
ARG CI_IMAGE_TAG=:ubuntu22.04-3
ARG BUILD_IMAGE_TAG

FROM phusion/baseimage:jammy-1.0.1 AS runtime

ENV LANG=en_US.UTF-8

SHELL ["/bin/bash", "-c"] 

USER root
WORKDIR /usr/local/src
ADD ./scripts/setup_ubuntu.sh /usr/local/src/scripts/

# Install base runtime packages
RUN ./scripts/setup_ubuntu.sh --runtime --hived-admin-account="hived_admin" --hived-account="hived"

USER hived_admin
WORKDIR /home/hived_admin

FROM ${CI_REGISTRY_IMAGE}runtime$CI_IMAGE_TAG AS ci-base-image

ENV LANG=en_US.UTF-8
ENV PATH="/home/hived_admin/.local/bin:$PATH"

SHELL ["/bin/bash", "-c"] 

USER root
WORKDIR /usr/local/src

COPY scripts/openssl.conf /etc/ssl/hive-openssl.conf

# Install additionally development packages
# TODO REMOVE the additional openssl configuaration when OpenSSL 3.0.7 or above will be distributed by Ubuntu.
RUN echo -e "\n.include /etc/ssl/hive-openssl.conf\n" >> /etc/ssl/openssl.cnf && \
   ./scripts/setup_ubuntu.sh --dev --hived-admin-account="hived_admin" --hived-account="hived"

USER hived_admin
WORKDIR /home/hived_admin

# Install additionally packages located in user directory
RUN /usr/local/src/scripts/setup_ubuntu.sh --user

FROM ${CI_REGISTRY_IMAGE}ci-base-image$CI_IMAGE_TAG AS build

ARG BUILD_HIVE_TESTNET=OFF
ENV BUILD_HIVE_TESTNET=${BUILD_HIVE_TESTNET}

ARG ENABLE_SMT_SUPPORT=OFF
ENV ENABLE_SMT_SUPPORT=${ENABLE_SMT_SUPPORT}

ARG HIVE_CONVERTER_BUILD=OFF
ENV HIVE_CONVERTER_BUILD=${HIVE_CONVERTER_BUILD}

ARG HIVE_LINT=OFF
ENV HIVE_LINT=${HIVE_LINT}

USER hived_admin
WORKDIR /home/hived_admin
SHELL ["/bin/bash", "-c"] 

# Get everything from cwd as sources to be built.
COPY --chown=hived_admin:users . /home/hived_admin/hive

RUN \
  mkdir -p ./build/tests/unit/ \
  && ./hive/scripts/build.sh --source-dir="./hive" --binary-dir="./build" \
  --cmake-arg="-DBUILD_HIVE_TESTNET=${BUILD_HIVE_TESTNET}" \
  --cmake-arg="-DENABLE_SMT_SUPPORT=${ENABLE_SMT_SUPPORT}" \
  --cmake-arg="-DHIVE_CONVERTER_BUILD=${HIVE_CONVERTER_BUILD}" \
  --cmake-arg="-DHIVE_LINT=${HIVE_LINT}" \
  && \
  cd ./build && \
  find . -name *.o  -type f -delete && \
  find . -name *.a  -type f -delete

# Here we could use a smaller image without packages specific to build requirements
FROM ${CI_REGISTRY_IMAGE}ci-base-image$CI_IMAGE_TAG as base_instance

ENV BUILD_IMAGE_TAG=${BUILD_IMAGE_TAG}

ARG P2P_PORT=2001
ENV P2P_PORT=${P2P_PORT}

ARG WS_PORT=8091
ENV WS_PORT=${WS_PORT}

ARG HTTP_PORT=8090
ENV HTTP_PORT=${HTTP_PORT}

ARG CLI_WALLET_PORT=8093
ENV CLI_WALLET_PORT=${CLI_WALLET_PORT}

SHELL ["/bin/bash", "-c"]

USER hived
WORKDIR /home/hived

RUN mkdir -p /home/hived/datadir/ && mkdir /home/hived/shm_dir/

COPY --from=build --chown=hived:users \
  /home/hived_admin/build/programs/hived/hived \
  /home/hived_admin/build/programs/cli_wallet/cli_wallet \
  /home/hived_admin/build/programs/util/* \
  /home/hived_admin/build/programs/blockchain_converter/blockchain_converter* \
  /home/hived_admin/build/tests/unit/* /home/hived/bin/

COPY --from=build --chown=hived:users /home/hived_admin/hive/doc/example_config.ini /home/hived/datadir/example_config.ini

USER hived_admin
WORKDIR /home/hived_admin

COPY --from=build --chown=hived_admin:users /home/hived_admin/hive/scripts/common.sh ./scripts/common.sh

COPY --chown=hived_admin:users ./docker/docker_entrypoint.sh .

VOLUME [ "/home/hived/datadir", "/home/hived/shm_dir" ]

ENV DATADIR=/home/hived/datadir
ENV SHM_DIR=/home/hived/shm_dir

STOPSIGNAL SIGINT

# JSON rpc service
EXPOSE ${HTTP_PORT}

ENTRYPOINT [ "/home/hived_admin/docker_entrypoint.sh" ]

FROM ${CI_REGISTRY_IMAGE}base_instance:base_instance-${BUILD_IMAGE_TAG} as instance

#p2p service
EXPOSE ${P2P_PORT}
# websocket service
EXPOSE ${WS_PORT}
# JSON rpc service
EXPOSE ${HTTP_PORT}
# Port specific to HTTP cli_wallet server
EXPOSE ${CLI_WALLET_PORT}
