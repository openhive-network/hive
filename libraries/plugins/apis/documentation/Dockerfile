FROM registry.gitlab.syncad.com/hive/common-ci-configuration/mirrors/alpine-3.22.1 AS transmogrifier

RUN apk add --no-cache jq

COPY openapi.json /openapi.json.orig
COPY walletapi.json /walletapi.json.orig

ARG API_VERSION="0.0.0-dev"

# Minify and set version
RUN jq -c --arg v "$API_VERSION" '.info.version = $v' /openapi.json.orig > /openapi.json
RUN jq -c --arg v "$API_VERSION" '.info.version = $v' /walletapi.json.orig > /walletapi.json

# Start from busybox
FROM registry.gitlab.syncad.com/hive/common-ci-configuration/mirrors/busybox-1.37.0

# Set the working directory
WORKDIR /www

RUN echo .html:application/json > /etc/httpd.conf

# Copy your OpenAPI spec into the container
COPY --from=transmogrifier /openapi.json /www/openapi.json
COPY --from=transmogrifier /walletapi.json /www/walletapi.json

# Expose HTTP port
EXPOSE 8080

# Launch BusyBox's built-in HTTP server
CMD ["httpd", "-f"]

# Labels at the end for better layer caching
ARG API_VERSION="0.0.0-dev"
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
LABEL org.opencontainers.image.revision="$GIT_COMMIT_SHA"
LABEL org.opencontainers.image.licenses="MIT"
LABEL org.opencontainers.image.ref.name="Hive OpenAPI Spec"
LABEL org.opencontainers.image.title="Hive OpenAPI Specification Server"
LABEL org.opencontainers.image.description="Serves the OpenAPI specification for the Hive JSON-RPC and Wallet APIs"
LABEL org.opencontainers.image.version="$API_VERSION"
LABEL io.hive.image.branch="$GIT_CURRENT_BRANCH"
LABEL io.hive.image.commit.log_message="$GIT_LAST_LOG_MESSAGE"
LABEL io.hive.image.commit.author="$GIT_LAST_COMMITTER"
LABEL io.hive.image.commit.date="$GIT_LAST_COMMIT_DATE"
