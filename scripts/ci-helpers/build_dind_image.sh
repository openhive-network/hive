#! /bin/bash

export REGISTRY=${1:-registry.gitlab.syncad.com/hive/hive/}
export DOCKER_VERSION=20.10.10
export DIND_VERSION=${DOCKER_VERSION}-dind

export DOCKER_BUILDKIT=1

docker build -t ${REGISTRY}custom_docker:${DOCKER_VERSION} - << EOF
    FROM docker:${DOCKER_VERSION}
    RUN apk update && apk add --no-cache bash git ca-certificates curl jq
    RUN adduser -D hived
    USER hived
    WORKDIR /home/hived
EOF

docker build -t ${REGISTRY}custom_dind:${DIND_VERSION} - << EOF
    # To workaround a gitlab healthcheck bug, expose just single port. See https://gitlab.com/gitlab-org/gitlab-runner/-/issues/29130#note_1028331564

    FROM docker:${DIND_VERSION} as upstream

    # copy everything to a clean image, so we can change the exposed ports
    # see https://gitlab.com/search?search=Service+docker+dind+probably+didn%27t+start+properly&nav_source=navbar&project_id=250833&group_id=9970&scope=issues&sort=updated_desc
    FROM scratch

    COPY --from=upstream / /

    RUN apk update && apk --no-cache add bash ca-certificates curl jq

    VOLUME /var/lib/docker

    #EXPOSE 2375/tcp # is for insecure connections, and having both breaks Gitlab's "wait-for-it" service
    EXPOSE 2376/tcp

    ENTRYPOINT ["dockerd-entrypoint.sh"]
    CMD []

EOF

