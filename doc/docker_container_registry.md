# Docker Container Registry Documentation

This document describes the container registries under `registry.gitlab.syncad.com/hive/hive`.

## Active Registries

### Production Images

| Registry | Path | Description |
|----------|------|-------------|
| **(root)** | `hive/hive:TAG` | Main production hived images. Contains release versions (1.27.x, 1.28.x) and commit-based tags. This is the primary registry for deployment. |
| **instance** | `hive/hive/instance:TAG` | Alternative path for production hived instance images. Contains same release versions as root. |
| **mirrornet** | `hive/hive/mirrornet:TAG` | Mirrornet hived images for testing with mainnet-derived data. Used by mirrornet CI jobs. |
| **testnet** | `hive/hive/testnet:TAG` | Testnet hived images built with `BUILD_HIVE_TESTNET=ON`. |

### Base/Runtime Images

| Registry | Path | Description |
|----------|------|-------------|
| **runtime** | `hive/hive/runtime:TAG` | Base runtime image built from `phusion/baseimage`. Contains runtime dependencies. Used as base for `ci-base-image`. Tags: `ubuntu22.04-*`, `ubuntu24.04-*` |
| **minimal-runtime** | `hive/hive/minimal-runtime:TAG` | Minimal Ubuntu-based runtime image. Lighter than `runtime`. Used as base for production `instance` images in Dockerfile. |
| **ci-base-image** | `hive/hive/ci-base-image:TAG` | CI build environment with development packages. **DEPRECATED** - now built in `common-ci-configuration` repo. Legacy tags remain for backwards compatibility. |
| **ci-base-image-5m** | `hive/hive/ci-base-image-5m:TAG` | CI image with pre-replayed 5M block data. **DEPRECATED** - replay caching now handled differently. |
| **mirrornet-base** | `hive/hive/mirrornet-base:TAG` | Base image for mirrornet builds. |

### Test Data Images

| Registry | Path | Description |
|----------|------|-------------|
| **testing-block-logs** | `hive/hive/testing-block-logs:TAG` | Pre-generated block logs for CI testing. Tags identify the generator and checksum (e.g., `comment_cashout-<hash>`). Used by `QUICK_TEST_BLOCK_LOG_IMAGE` variable. |
| **universal-block-logs** | `hive/hive/universal-block-logs:TAG` | Universal block log images for various test scenarios. Tags follow pattern `universal-block-log-<hash>`. |
| **extended-block-log** | `hive/hive/extended-block-log:TAG` | Extended mirrornet block log data for longer replay tests. |

### API Specification Images

| Registry | Path | Description |
|----------|------|-------------|
| **openapi-spec** | `hive/hive/openapi-spec:TAG` | OpenAPI specification for hived APIs. Tags match hive versions. |
| **walletapi-spec** | `hive/hive/walletapi-spec:TAG` | Wallet API specification images. |

## Legacy/Unused Registries (Candidates for Deletion)

| Registry | Path | Description |
|----------|------|-------------|
| **builder** | `hive/hive/builder:TAG` | ??? Legacy build images from 2020. Only 7 tags. |
| **test** | `hive/hive/test:TAG` | ??? Legacy test images from 2020. Only 6 tags. |
| **python** | `hive/hive/python:TAG` | ??? Python base image. Single tag (`3.8-slim-bullseye`) from 2022. |
| **root_node** | `hive/hive/root_node:TAG` | ??? Single tag (`20221110_manual`) from 2022. Purpose unclear. |
| **custom_docker** | `hive/hive/custom_docker:TAG` | ??? Custom Docker client image. Single tag (`20.10.10-1`). Not referenced in codebase. |
| **custom_dind** | `hive/hive/custom_dind:TAG` | ??? Custom Docker-in-Docker image. Single tag (`20.10.10-dind-1`). Not referenced in codebase. |
| **base** | `hive/hive/base:TAG` | ??? Single tag from image rename effort (Jan 2025). Never integrated. |
| **hive-baseenv** | `hive/hive/hive-baseenv:TAG` | ??? Very old (2020) base environment image. Single `latest` tag. Superseded by `ci-base-image`. |

## Recently Deleted Registries

The following registries were deleted as unused:

- `testnet-base_instance` (488) - Legacy testnet base image, never integrated
- `testnet-base` (661) - From rename effort, never integrated
- `minimal` (662) - From rename effort, never integrated
- `consensus_node` (321) - Very old (2022), single tag, not referenced
- `minimal-instance` (602) - Superseded by root `hive/hive:TAG` images

## Image Hierarchy (Dockerfile)

```
phusion/baseimage:noble-1.0.1
    └── runtime (hive/hive/runtime:TAG)
            └── ci-base-image (now in common-ci-configuration)
                    └── build (intermediate, not published)

ubuntu:24.04
    └── minimal-runtime (hive/hive/minimal-runtime:TAG)
            └── instance (hive/hive/instance:TAG or hive/hive:TAG)
```

## Usage Examples

```bash
# Pull latest production image
docker pull registry.gitlab.syncad.com/hive/hive:1.28.5

# Pull specific testnet image
docker pull registry.gitlab.syncad.com/hive/hive/testnet:e3ddfbd1

# Pull mirrornet image
docker pull registry.gitlab.syncad.com/hive/hive/mirrornet:latest

# Pull CI base image (deprecated - use common-ci-configuration)
docker pull registry.gitlab.syncad.com/hive/hive/ci-base-image:ubuntu24.04-py3.14-1
```

## CI Variables

- `CI_REGISTRY_IMAGE`: `registry.gitlab.syncad.com/hive/hive`
- `CI_BASE_IMAGE`: Now points to `registry.gitlab.syncad.com/hive/common-ci-configuration/ci-base-image`
- `QUICK_TEST_BLOCK_LOG_IMAGE`: Use `testing-block-logs` images for quick test mode
