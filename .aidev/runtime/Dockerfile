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
USER hived

ENTRYPOINT []
CMD ["bash"]
