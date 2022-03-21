#Usage: DOCKER_BUILDKIT=1 docker build --no-cache  --target=testnet_node_builder .

ARG BUILD_HIVE_TESTNET=OFF
ARG HIVE_LINT=OFF
FROM registry.gitlab.syncad.com/hive/hive/builder:ubuntu-20.04 AS builder

ENV src_dir="/usr/local/src/hive"
ENV install_base_dir="/usr/local/hive"
ENV LANG=en_US.UTF-8

ADD . ${src_dir}
WORKDIR ${src_dir}

###################################################################################################
##                                        CONSENSUS NODE BUILD                                   ##
###################################################################################################

FROM builder AS consensus_node_builder

RUN \
  cd ${src_dir} && \
    ${src_dir}/ciscripts/build.sh "OFF" "ON"

###################################################################################################
##                                  CONSENSUS NODE CONFIGURATION                                 ##
###################################################################################################

FROM builder AS consensus_node
ARG TRACKED_ACCOUNT_NAME
ENV TRACKED_ACCOUNT_NAME=${TRACKED_ACCOUNT_NAME}
ARG USE_PUBLIC_BLOCKLOG
ENV USE_PUBLIC_BLOCKLOG=${USE_PUBLIC_BLOCKLOG}

WORKDIR "${install_base_dir}/consensus"
# Get all needed files from previous stage, and throw away unneeded rest(like objects)
COPY --from=consensus_node_builder ${src_dir}/build/install-root/ ${src_dir}/contrib/hived.run ./
COPY --from=consensus_node_builder ${src_dir}/contrib/config-for-docker.ini  datadir/config.ini

RUN \
   mkdir -p datadir/blockchain && ls -la && \
   chmod +x hived.run

# rpc service :
EXPOSE 8090
# p2p service :
EXPOSE 2001
CMD "${install_base_dir}/consensus/hived.run"

###################################################################################################
##                                          GENERAL NODE BUILD                                   ##
###################################################################################################

FROM builder AS general_node_builder

ARG BUILD_HIVE_TESTNET
ARG HIVE_LINT

ENV BUILD_HIVE_TESTNET=${BUILD_HIVE_TESTNET}
ENV HIVE_LINT=${HIVE_LINT}

RUN \
  cd ${src_dir} && \
    ${src_dir}/ciscripts/build.sh ${BUILD_HIVE_TESTNET} ${HIVE_LINT}

###################################################################################################
##                                    GENERAL NODE CONFIGURATION                                 ##
###################################################################################################

FROM builder AS general_node
ARG TRACKED_ACCOUNT_NAME
ENV TRACKED_ACCOUNT_NAME=${TRACKED_ACCOUNT_NAME}
ARG USE_PUBLIC_BLOCKLOG
ENV USE_PUBLIC_BLOCKLOG=${USE_PUBLIC_BLOCKLOG}

WORKDIR "${install_base_dir}/hive-node"
# Get all needed files from previous stage, and throw away unneeded rest(like objects)
COPY --from=general_node_builder ${src_dir}/build/install-root/ ${src_dir}/contrib/hived.run ./
COPY --from=general_node_builder ${src_dir}/contrib/config-for-docker.ini  datadir/config.ini

RUN \
   mkdir -p datadir/blockchain && ls -la && \
   chmod +x hived.run

# rpc service :
EXPOSE 8090
# p2p service :
EXPOSE 2001
CMD "${install_base_dir}/hive-node/hived.run"

###################################################################################################
##                                          TESTNET NODE BUILD                                   ##
###################################################################################################

FROM builder AS testnet_node_builder

ARG HIVE_LINT=ON

ENV BUILD_HIVE_TESTNET="ON"
ENV HIVE_LINT=${HIVE_LINT}

RUN \
  cd ${src_dir} && \
      apt-get update && \
      apt-get install -y clang && \
      apt-get install -y clang-tidy && \
      ${src_dir}/ciscripts/build.sh ${BUILD_HIVE_TESTNET} ${HIVE_LINT} && \
      apt-get install -y screen && \
      pip3 install -U secp256k1prp && \
      git clone https://gitlab.syncad.com/hive/beem.git && \
      cd beem && \
        git checkout dk-update-proposal-operation && \
        python3 setup.py build && \
        python3 setup.py install --user && \
  cd ${src_dir} && \
        ${src_dir}/ciscripts/run_regressions.sh
