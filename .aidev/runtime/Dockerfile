# The image AIDEV runs this project's checks in (`environment.image` in
# .aidev/project.yaml): CI's own build image (ci-base-image, as in
# scripts/ci-helpers/ci_image_tag_vars.yml), so a check compiles hived with the
# toolchain, Boost and sccache CI uses — and with CI's compile flags and paths it
# shares CI's compiler cache. Only the entrypoint is cleared, so AIDEV's argv
# (.aidev/run-checks.sh …) runs as given.
#
# Re-pin when CI_BASE_IMAGE_TAG moves, then rebuild with .aidev/runtime/build.sh.
FROM registry.gitlab.syncad.com/hive/common-ci-configuration/ci-base-image@sha256:3c6277d480e062cd34195ebdb6a4539484b270a80efe08daac3a3326705008a6

# The `wallet-patterns` step's test-tools (.aidev/README.md): the suites run with no
# network, so the packages tests/python/hive-local-tools/poetry.lock pins for its
# pytest run are installed here, at the lock's versions; the checkout's own
# hive_local_tools and tests_api come from PYTHONPATH. Bump these with the lock.
USER root
RUN python3 -m venv /opt/test-tools-venv \
    && /opt/test-tools-venv/bin/pip install --no-cache-dir \
        --extra-index-url https://gitlab.syncad.com/api/v4/groups/136/-/packages/pypi/simple \
        hiveio-test-tools==1.28.8rc2.post20261002091953 \
        hiveio-wax==1.28.8rc2.post20261002091953 \
        hiveio-beekeepy==1.28.6.post20260506200636 \
        hiveio-api==1.28.8rc3 hiveio-schemas==1.28.2.post20260313095431 \
        pytest==9.0.2 pytest-xdist==3.8.0 pytest-timeout==2.4.0 pytest-rerunfailures==14.0 \
        pytest-asyncio==1.3.0 pytest-repeat==0.9.4 \
        prettytable==3.8.0 requests==2.32.3 deepdiff==6.3.0 setuptools==80.9.0 \
    && cd /tmp && /opt/test-tools-venv/bin/python3 -c 'import test_tools' && rm -rf /tmp/generated
# The import above is not only a check: wax links its extension modules into
# site-packages on first import, which a suite's unprivileged user cannot.
USER hived

ENTRYPOINT []
CMD ["bash"]
