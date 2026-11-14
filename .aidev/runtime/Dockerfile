# The image AIDEV runs this project's checks in. AIDEV builds it on the
# ci-base-image tag .aidev/project.yaml's `environment.base` follows (CI's own
# build image, as in scripts/ci-helpers/ci_image_tag_vars.yml), passing that
# tag's current digest as BASE_IMAGE, so a check compiles hived with the
# toolchain, Boost and sccache CI uses - and with CI's compile flags and paths it
# shares CI's compiler cache. Only the entrypoint is cleared, so AIDEV's argv
# (.aidev/run-checks.sh ...) runs as given.
#
# The build context is the checkout (`environment.build`), of which
# Dockerfile.dockerignore beside this file admits only the files read below. The
# steps below read it through a bind mount, not COPY, so that this file also builds
# on tests/python/hive-local-tools, the context approved before: AIDEV keeps an
# approved `environment.build` until a maintainer approves its change. That context
# holds no API generator, so the generator environment is then left out and the
# `hiveio-api` step is skipped as api_generation_unavailable.
ARG BASE_IMAGE
FROM ${BASE_IMAGE}

# The `wallet-patterns` step's test-tools (.aidev/README.md): the suites run with no
# network, so the main group of poetry.lock - what CI's pytest jobs install - is
# installed here, at the lock's versions; the checkout's own hive_local_tools and
# tests_api come from PYTHONPATH.
USER root
RUN --mount=type=bind,target=/context \
    tools=/context/tests/python/hive-local-tools \
    && { [ -f "$tools/poetry.lock" ] || tools=/context; } \
    && mkdir -p /opt/hive-local-tools \
    && cp -r "$tools/pyproject.toml" "$tools/poetry.lock" "$tools/tests_api" /opt/hive-local-tools/ \
    && python3 -m venv /opt/test-tools-venv \
    && VIRTUAL_ENV=/opt/test-tools-venv poetry -C /opt/hive-local-tools install --only main --no-root --no-interaction \
    && cd /tmp && /opt/test-tools-venv/bin/python3 -c 'import test_tools' && rm -rf /tmp/generated
# The import above is not only a check: wax links its extension modules into
# site-packages on first import, which a suite's unprivileged user cannot.

# The `hiveio-api` step's generator environment (.aidev/README.md), for the same
# reason: the main group of the generator's poetry.lock, the pytest of the
# test-tools lock above (CI takes python-test-tools' constraints), and the build
# backend the generated package's pyproject.toml requires, so that its wheel builds
# without build isolation.
RUN --mount=type=bind,target=/context \
    gen=/context/libraries/plugins/apis/api_generation \
    && if [ ! -f "$gen/api_generation/poetry.lock" ]; then \
        echo "the build context holds no API generator: /opt/api-generation-venv is left out"; exit 0; fi \
    && mkdir -p /opt/api-generation/python_api_package \
    && cp "$gen/api_generation/pyproject.toml" "$gen/api_generation/poetry.lock" /opt/api-generation/ \
    && cp "$gen/python_api_package/pyproject.toml" /opt/api-generation/python_api_package/ \
    && python3 -m venv /opt/api-generation-venv \
    && VIRTUAL_ENV=/opt/api-generation-venv poetry -C /opt/api-generation install --only main --no-root --no-interaction \
    && /opt/api-generation-venv/bin/python3 -m pip install --no-cache-dir \
        "$(/opt/test-tools-venv/bin/python3 -m pip freeze | grep '^pytest==')" \
        $(/opt/api-generation-venv/bin/python3 -c 'import tomllib; print(*tomllib.load(open("/opt/api-generation/python_api_package/pyproject.toml", "rb"))["build-system"]["requires"])') \
    && /opt/api-generation-venv/bin/python3 -c 'import api_client_generator, jinja2, pytest, poetry_dynamic_versioning'

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
