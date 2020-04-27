#Usage: DOCKER_BUILDKIT=1 docker build --no-cache  --target=testnet_node_builder .

ARG LOW_MEMORY_NODE=ON
ARG CLEAR_VOTES=ON
ARG BUILD_STEEM_TESTNET=OFF
ARG ENABLE_MIRA=OFF
FROM registry.gitlab.syncad.com/hive/hive/hive-baseenv:latest AS builder

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
    ${src_dir}/ciscripts/build.sh "ON" "ON" "OFF" "OFF"

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
   ls -la && \
   chmod +x hived.run

# rpc service :
EXPOSE 8090
# p2p service :
EXPOSE 2001
CMD "${install_base_dir}/consensus/hived.run"

###################################################################################################
##                                            FAT NODE BUILD                                     ##
###################################################################################################

FROM builder AS fat_node_builder

RUN \
  cd ${src_dir} && \
    ${src_dir}/ciscripts/build.sh "OFF" "OFF" "OFF" "ON"

###################################################################################################
##                                      FAT NODE CONFIGURATION                                   ##
###################################################################################################

FROM builder AS fat_node
ARG TRACKED_ACCOUNT_NAME
ENV TRACKED_ACCOUNT_NAME=${TRACKED_ACCOUNT_NAME}
ARG USE_PUBLIC_BLOCKLOG
ENV USE_PUBLIC_BLOCKLOG=${USE_PUBLIC_BLOCKLOG}

WORKDIR "${install_base_dir}/fat-node"
# Get all needed files from previous stage, and throw away unneeded rest(like objects)
COPY --from=fat_node_builder ${src_dir}/build/install-root/ ${src_dir}/contrib/hived.run ./
COPY --from=fat_node_builder ${src_dir}/contrib/config-for-docker.ini  datadir/config.ini

RUN \
   ls -la && \
   chmod +x hived.run

# rpc service :
EXPOSE 8090
# p2p service :
EXPOSE 2001
CMD "${install_base_dir}/fat-node/hived.run"

###################################################################################################
##                                          GENERAL NODE BUILD                                   ##
###################################################################################################

FROM builder AS general_node_builder

ARG LOW_MEMORY_NODE
ARG CLEAR_VOTES
ARG BUILD_STEEM_TESTNET
ARG ENABLE_MIRA

ENV LOW_MEMORY_NODE=${LOW_MEMORY_NODE}
ENV CLEAR_VOTES=${CLEAR_VOTES}
ENV BUILD_STEEM_TESTNET=${BUILD_STEEM_TESTNET}
ENV ENABLE_MIRA=${ENABLE_MIRA}

RUN \
  cd ${src_dir} && \
    ${src_dir}/ciscripts/build.sh ${LOW_MEMORY_NODE} ${CLEAR_VOTES} ${BUILD_STEEM_TESTNET} ${ENABLE_MIRA}

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
   ls -la && \
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

ARG LOW_MEMORY_NODE=OFF
ARG CLEAR_VOTES=OFF
ARG ENABLE_MIRA=OFF

ENV LOW_MEMORY_NODE=${LOW_MEMORY_NODE}
ENV CLEAR_VOTES=${CLEAR_VOTES}
ENV BUILD_STEEM_TESTNET="ON"
ENV ENABLE_MIRA=${ENABLE_MIRA}

RUN \
  cd ${src_dir} && \
    ${src_dir}/ciscripts/build.sh ${LOW_MEMORY_NODE} ${CLEAR_VOTES} ${BUILD_STEEM_TESTNET} ${ENABLE_MIRA} && \
    cd build/tests && \
    ./chain_test && \
    ./plugin_test 

