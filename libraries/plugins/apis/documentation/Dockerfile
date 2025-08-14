FROM alpine:3.22.1 AS transmogrifier

RUN apk add --no-cache jq

COPY openapi.json /openapi.json.orig
# set server to correct url
RUN jq -c '.servers=[{url:"/"}]' /openapi.json.orig > /openapi.json

# Start from busybox
FROM busybox:1.37.0

# Set the working directory
WORKDIR /www

RUN echo .html:application/json > /etc/httpd.conf

# Copy your OpenAPI spec into the container
COPY --from=transmogrifier /openapi.json /www/openapi.json

# Expose HTTP port
EXPOSE 8080

# Launch BusyBox's built-in HTTP server
CMD ["httpd", "-f"]

