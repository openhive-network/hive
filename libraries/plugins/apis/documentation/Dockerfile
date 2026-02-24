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
