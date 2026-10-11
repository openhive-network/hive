# The image AIDEV runs this project's checks in (`environment.image` in
# .aidev/project.yaml): CI's own build image (ci-base-image, as in
# scripts/ci-helpers/ci_image_tag_vars.yml), so a check compiles hived with the
# toolchain, Boost and sccache CI uses — and with CI's compile flags and paths it
# shares CI's compiler cache. Only the entrypoint is cleared, so AIDEV's argv
# (.aidev/run-checks.sh …) runs as given.
#
# Re-pin when CI_BASE_IMAGE_TAG moves, then rebuild with .aidev/runtime/build.sh.
FROM registry.gitlab.syncad.com/hive/common-ci-configuration/ci-base-image@sha256:3c6277d480e062cd34195ebdb6a4539484b270a80efe08daac3a3326705008a6

ENTRYPOINT []
CMD ["bash"]
