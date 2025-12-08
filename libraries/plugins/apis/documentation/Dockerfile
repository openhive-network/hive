FROM alpine:3.22.1 AS transmogrifier

RUN apk add --no-cache jq

COPY openapi.json /openapi.json.orig
COPY walletapi.json /walletapi.json.orig

# Minify the JSON files
RUN jq -c '.' /openapi.json.orig > /openapi.json
RUN jq -c '.' /walletapi.json.orig > /walletapi.json

# Start from busybox
FROM busybox:1.37.0

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
