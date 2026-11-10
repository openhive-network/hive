# The image AIDEV runs this project's checks in. AIDEV builds it on the
# ci-base-image tag .aidev/project.yaml's `environment.base` follows (CI's own
# build image, as in scripts/ci-helpers/ci_image_tag_vars.yml), passing that
# tag's current digest as BASE_IMAGE, so a check compiles hived with the
# toolchain, Boost and sccache CI uses - and with CI's compile flags and paths it
# shares CI's compiler cache. Only the entrypoint is cleared, so AIDEV's argv
# (.aidev/run-checks.sh ...) runs as given.
#
# The build context is tests/python/hive-local-tools (`environment.build`).
ARG BASE_IMAGE
FROM ${BASE_IMAGE}

# The `wallet-patterns` step's test-tools (.aidev/README.md): the suites run with no
# network, so the main group of poetry.lock - what CI's pytest jobs install - is
# installed here, at the lock's versions; the checkout's own hive_local_tools and
# tests_api come from PYTHONPATH.
USER root
COPY pyproject.toml poetry.lock /opt/hive-local-tools/
COPY tests_api /opt/hive-local-tools/tests_api
RUN python3 -m venv /opt/test-tools-venv \
    && VIRTUAL_ENV=/opt/test-tools-venv poetry -C /opt/hive-local-tools install --only main --no-root --no-interaction \
    && cd /tmp && /opt/test-tools-venv/bin/python3 -c 'import test_tools' && rm -rf /tmp/generated
# The import above is not only a check: wax links its extension modules into
# site-packages on first import, which a suite's unprivileged user cannot.

# The `integration` step's docker compose (test_snapshot_bootstrap.sh). The base
# brings the docker CLI (29.9.2) and buildx as static release binaries, and
# Docker's el8 repository stops at docker-compose-plugin 2.27, so this is the
# static release binary of the compose series current with that CLI. A host's own
# plugin is no substitute: Ubuntu's packages before noble link glibc 2.32+, and
# the base (AlmaLinux 8) has 2.28.
ARG COMPOSE_VERSION=v5.6.1
ARG COMPOSE_SHA256=47737f4236e6d3ccea7291995203f8f018058783b4c3a9469119811a0b5db8c0
RUN curl -fsSL -o /usr/libexec/docker/cli-plugins/docker-compose \
        "https://github.com/docker/compose/releases/download/${COMPOSE_VERSION}/docker-compose-linux-x86_64" \
    && echo "${COMPOSE_SHA256}  /usr/libexec/docker/cli-plugins/docker-compose" | sha256sum -c - \
    && chmod 755 /usr/libexec/docker/cli-plugins/docker-compose \
    && docker compose version
USER hived

ENTRYPOINT []
CMD ["bash"]
