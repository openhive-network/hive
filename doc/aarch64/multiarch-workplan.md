# Workplan — Multiarch `hived` image (x64 + aarch64) and ARM-ready `ci-base-image`

Covers GitLab work items:
- **[#831](https://gitlab.syncad.com/hive/hive/-/work_items/831)** — Add ARM toolchain to `ci-base-image` using buildroot.
- **[#832](https://gitlab.syncad.com/hive/hive/-/work_items/832)** — Build multiarch `hived` image (x64 + aarch64).

Parent task list: `hive/tasks_without_projects_yet#55`.

---

## 1. How the two issues fit together

There are two ways to produce an `aarch64` Docker image with BuildKit:

| Strategy | Mechanism | Verdict for `hived` |
|----------|-----------|---------------------|
| **A. QEMU emulation** | `buildx --platform linux/arm64` runs the whole `build` stage emulated on the x64 builder | ❌ Compiling hived (Boost + RocksDB + chain) under qemu is many hours and memory-heavy — not CI-viable |
| **B. Cross-compilation** | Build natively on x64, cross-compile to aarch64 with a prebuilt cross toolchain, package binaries into a thin aarch64 runtime image | ✅ Fast (native compile speed), reproducible — this is the path the `doc/aarch64` material was created for |

**Therefore #831 is the enabler for #832.** The buildroot cross toolchain must live inside `ci-base-image` so the multiarch build can cross-compile the aarch64 binaries on an x64 builder, then assemble & publish a multiarch manifest. Do #831 first.

Current build topology (from [Dockerfile](../../Dockerfile) and [build_instance.sh](../../scripts/ci-helpers/build_instance.sh)):

```
build stage     FROM ci-base-image (x64)      -> build.sh native compile  ─┐
                                                                            ├─> instance stage FROM minimal-runtime (ubuntu24.04)
runtime bases   phusion/baseimage, ubuntu:24.04 -> setup_ubuntu.sh --runtime ┘     COPY binaries from build
```

`ci-base-image` is **not** in this repo — it is built in `hive/common-ci-configuration` from `Dockerfile.ci-base-image` (bake target `ci-base-image`, version `pypa_2_28-pg18-3`) and pinned here by commit hash in [Dockerfile](../../Dockerfile) and [ci_image_tag_vars.yml](../../scripts/ci-helpers/ci_image_tag_vars.yml). It is a manylinux_2_28 / AlmaLinux 8 **x86_64** image carrying static Boost 1.88, OpenSSL 3.0.13, prebuilt RocksDB, snappy, sccache, Pythons, Docker CLI + buildx — every download in it is x86_64-specific.

---

## 2. Part A — Issue #831: ARM-ready `ci-base-image`

Repo: **`hive/common-ci-configuration`** (cloned locally at `/storage1/src/common-ci-configuration`).

The cross toolchain is built with **buildroot 2025.02.4** → GCC 14.2.0 / binutils 2.43.1 / **glibc downgraded to 2.35** (so binaries run on a wide range of arm64 runtimes), plus aarch64 target builds of Boost, OpenSSL, snappy, zlib, bzip2, readline, liburing, icu in the sysroot.

### A1. Decouple the toolchain build from `ci-base-image` (recommended)
Building buildroot inside the `ci-base-image` Docker build adds ~1 h to an image that is otherwise cheap and frequently rebuilt. Instead:
1. Add a CI job (in `common-ci-configuration`, or a one-off) that runs the buildroot build and **packs `output/host` into a versioned tarball** (`aarch64-toolchain-<gcc>-glibc2.35-<date>.tar.zst`), published as a registry/release artifact. The issue explicitly says "pack built toolchain ... for further use".
2. Pin the tarball by version + checksum.

### A2. Extend `Dockerfile.ci-base-image`
- Add a stage that downloads & unpacks the toolchain tarball to a **fixed, non-developer-specific path**, e.g. `/opt/hive/cross/aarch64`.
- `COPY` it into the final image.
- Cross-build & install the aarch64 RocksDB into a parallel commit-indexed prefix so the cross build also skips RocksDB compilation (mirror the existing x64 `rocksdb-builder` stage).
- Bump `CI_BASE_IMAGE_VERSION` (e.g. `pypa_2_28-pg18-arm-1`) in `docker-bake.hcl`.

### A3. Fix the toolchain wiring (see §4 — these are blocking bugs for any container use)
- Replace the hardcoded `CROSS_ROOT /storage1/src/buildroot/output/host` in [Toolchain.cmake](Toolchain.cmake) with an env-overridable path defaulting to the baked location.
- Ship `Toolchain.cmake` at the baked path (`$CROSS_ROOT/Toolchain.cmake`).
- Install buildroot/qemu host prerequisites in the image (or confirm manylinux already has them).

### A4. Consume the new image in hive
- Update `CI_BASE_IMAGE` (commit hash) in [Dockerfile](../../Dockerfile) and `CI_BASE_IMAGE_TAG` in [ci_image_tag_vars.yml](../../scripts/ci-helpers/ci_image_tag_vars.yml) once published.

**Acceptance (#831):** buildroot config + patches captured (already in `doc/aarch64/buildroot`); toolchain build integrated & packed; new `ci-base-image` contains the prebuilt ARM toolchain; cross-compilation of a sample target verified.

---

## 3. Part B — Issue #832: Multiarch `hived` image

### B1. Make the runtime base images multiarch first (dependency)
The `instance` stage does `FROM ${CI_REGISTRY_IMAGE}minimal-runtime:$CI_IMAGE_TAG`. That prebuilt `minimal-runtime` (and `runtime`) must be published as a **multiarch manifest** (amd64 + arm64). Both derive from `ubuntu:24.04` / `phusion/baseimage:noble` which publish arm64, and `setup_ubuntu.sh --runtime` is apt-based, so building these two stages for arm64 under QEMU is cheap (apt only). Build & push them with `buildx --platform linux/amd64,linux/arm64`.

### B2. Convert the `build` stage to cross-compile, never emulate
In [Dockerfile](../../Dockerfile), pin the build stage to the **native builder** and branch on target arch:
```dockerfile
FROM --platform=$BUILDPLATFORM ${CI_BASE_IMAGE} AS build
ARG TARGETARCH            # amd64 | arm64
# arm64  -> build.sh with --toolchain=$CROSS_ROOT/Toolchain.cmake (cross, native speed)
# amd64  -> build.sh as today (native)
```
- Unify `build_arm.sh` logic into `scripts/build.sh` (add a `--toolchain=` passthrough + the `CROSS_*`/`QEMU_*` env exports) so one code path serves both arches.
- The `instance` stage becomes `FROM --platform=$TARGETPLATFORM ...minimal-runtime` and copies the (cross-)built binaries — its arch now follows the manifest entry.

### B3. Drive multiarch from `build_instance.sh`
Add a `--platform`/multiarch mode that runs a single `buildx build --platform linux/amd64,linux/arm64 ... --push` producing one manifest. Note `--load` cannot load a multiarch image into the local daemon → multiarch builds must `--push` (or `--output type=oci`). Keep the existing single-arch `--load` path for local dev.

### B4. CI integration — multiarch build *on demand only*
The x64 `prepare_hived_image` keeps running every pipeline (it feeds the x64 5M-replay
test job). The expensive aarch64 + manifest build is a **separate job gated by `rules:`**
so it does NOT run on feature-branch / MR pipelines — only on develop, protected tags,
or a manual/variable trigger:

```yaml
prepare_hived_image_multiarch:
  extends: .prepare_hived_image       # reuse the builder, add --platform + --push
  variables:
    HIVE_BUILD_PLATFORMS: "linux/amd64,linux/arm64"
  rules:
    - if: '$CI_COMMIT_BRANCH == $CI_DEFAULT_BRANCH'   # develop
    - if: '$CI_COMMIT_TAG'                            # release/protected tags
    - if: '$BUILD_MULTIARCH == "true"'               # manual on-demand override
      when: manual
    - when: never                                    # all other (MR/feature) pipelines
```
(`$CI_COMMIT_REF_PROTECTED == "true"` is a one-line alternative covering
develop+master+protected tags.) Net: **x64 every pipeline; multiarch only for develop,
protected tags, or an explicit manual run** — keeping per-MR pipelines fast and avoiding
arm pushes on feature branches.

- Multiarch images must `--push` (cannot `--load` a manifest list into the local daemon);
  keep the existing single-arch `--load` path for local dev.
- The builder/runner needs `binfmt`/qemu registered only for the (emulated, cheap) arm64
  *runtime* stage — confirm `docker-builder`/`docker-dind` provide it.
- The aarch64 cross-build sets `-DHIVE_VENDOR_PREINSTALLED_DIR=/opt/hive/vendor-aarch64`
  so hive's CMake finds the preinstalled aarch64 RocksDB baked into ci-base-image (see §7).

### B5. Validate aarch64 at runtime
Per [building.md](../building.md): preferred path is to generate a snapshot on x64, copy to an arm64 host, and load it. Add a smoke test that boots the aarch64 image (`--version`, short replay) on an arm64 runner or under emulation.

**Acceptance (#832):** Dockerfile supports multi-arch; hived builds & smoke-tests on x64 and aarch64; multiarch manifest published to the registry.

---

## 4. Procedure verification — `doc/aarch64/buildroot` + `doc/building.md`

Verified by actually executing the procedure on this machine (buildroot 2025.02.4, toolchain build in progress). **The procedure is substantially correct but has gaps that block reproducibility, especially in a container:**

| # | Finding | Severity | Fix |
|---|---------|----------|-----|
| 1 | **Patches are never applied.** `building.md` says to *copy* `glibc_downgrade.patch` / `snappy_static_build.patch` into `buildroot/`, but they patch buildroot's own `package/glibc/*` and `package/snappy/snappy.mk` and must be applied. Verified they apply cleanly with `git apply`. As written, the copy is a no-op (`BR2_GLOBAL_PATCH_DIR=""`). | **High** | Add explicit `git apply glibc_downgrade.patch snappy_static_build.patch` step (or set `BR2_GLOBAL_PATCH_DIR`). |
| 2 | **qemu binary name mismatch.** [Toolchain.cmake](Toolchain.cmake) sets `CMAKE_CROSSCOMPILING_EMULATOR=/usr/bin/qemu-aarch64`, but Ubuntu's `qemu-user-static` installs `/usr/bin/qemu-aarch64-static`. Procedure never mentions installing qemu. | **Med** | Document `apt install qemu-user-static` + symlink, or install `qemu-user`, or point the var at `-static`. |
| 3 | **Hardcoded absolute `CROSS_ROOT`.** Toolchain.cmake hardcodes `/storage1/src/buildroot/output/host` (a developer's path). `build_arm.sh` exports `CROSS_ROOT` but Toolchain.cmake overrides it, so the env var is ignored. Breaks any other checkout location and all container use. | **High** | Make Toolchain.cmake honor `$CROSS_ROOT` (`if(DEFINED ENV{CROSS_ROOT})`), default to the baked image path. |
| 4 | **Undocumented host prerequisites.** buildroot needs `cpio`, `unzip`, `bc` (plus `rsync`, `file`, `wget`); none are in `scripts/setup_ubuntu.sh`. Build fails early without them. | **Med** | List buildroot host deps in `building.md` and/or `setup_ubuntu.sh`. |
| 5 | `<src-root>` is ambiguous in `building.md` (parent of `buildroot/` in some lines, repo root in others). | Low | Use explicit paths in the doc. |
| 6 | Otherwise accurate: tag `2025.02.4` matches `.config` header (`2025.02.4-dirty`); GCC 14.2.0 / binutils 2.43.1 / glibc 2.35; copy-`Toolchain.cmake`-after-`make` ordering is correct (`output/host` exists only post-build); `make menuconfig` correctly optional. | — | — |

Items 1–4 are exactly the steps that must be encoded as Dockerfile `RUN` lines for #831, so fixing them serves both the docs and the image.

---

## 5. Risks & open questions
- **Image size:** baking `output/host` (host toolchain + aarch64 sysroot) adds hundreds of MB to `ci-base-image`. Mitigate by packing only what's needed and using the tarball-download approach (A1).
- **glibc 2.35 vs runtime:** binaries link buildroot glibc 2.35; ubuntu:24.04 arm64 ships glibc 2.39 (forward-compatible). Confirm the chosen `minimal-runtime` arm64 base satisfies ≥2.35 and that `file`/`ldd` on the artifact look sane.
- **RocksDB cross build:** the x64 image preinstalls RocksDB by submodule commit so hive skips its 2000-file build; replicate this for aarch64 or accept a slower cross build.
- **binfmt on runners:** the emulated arm64 runtime stage + multiarch push need qemu/binfmt on the CI builder.

---

## 6. Sequencing / milestones
1. **M0 (this session):** verify procedure, build the buildroot toolchain locally, cross-build `hived` from current `develop`. ← in progress.
2. **M1 (#831):** fix Toolchain.cmake portability + document prerequisites/patch-apply; CI job builds & packs the toolchain tarball.
3. **M2 (#831):** extend `Dockerfile.ci-base-image`, bump version, publish; repoint hive's `CI_BASE_IMAGE`.
4. **M3 (#832):** publish multiarch `minimal-runtime`/`runtime`; cross-compile branch in Dockerfile + `build.sh`.
5. **M4 (#832):** multiarch mode in `build_instance.sh`; CI publish job for the manifest (protected branches/tags).
6. **M5 (#832):** aarch64 runtime smoke test; document deployment.

> Dependency chain: **M1 → M2 → M3 → M4 → M5.** #831 (M1–M2) gates #832 (M3–M5).

---

## 7. Implementation status (#831 — done in `common-ci-configuration`)

The ARM-toolchain-in-`ci-base-image` work (M1–M2) is implemented in the
`hive/common-ci-configuration` repo:

- **`aarch64-toolchain/`** — self-contained toolchain definition: `buildroot.config`,
  `patches/` (`glibc_downgrade.patch`, `snappy_static_build.patch`,
  **`boost_1_88_upgrade.patch`**), portable `Toolchain.cmake`, `build-toolchain.sh`, README.
- **`Dockerfile.ci-base-image`** — new `aarch64-toolchain-builder` stage (builds buildroot
  as a non-root user, applies config+patches, `make`), then
  `COPY --from=aarch64-toolchain-builder /opt/hive/cross /opt/hive/cross` into the final
  stage with `ENV CROSS_ROOT=/opt/hive/cross/aarch64`.
- **`aarch64-rocksdb-builder` stage** — cross-compiles the vendored RocksDB (same submodule
  commit + flags as the x64 `rocksdb-builder`) and bakes it to `/opt/hive/vendor-aarch64/rocksdb/<commit>`
  (+ `ENV HIVE_AARCH64_VENDOR_DIR`), so aarch64 hived builds skip the ~2000-file RocksDB
  compile — the aarch64 counterpart of the x64 preinstalled vendor libs. Together with the
  buildroot sysroot (Boost 1.88, OpenSSL, snappy, zlib, bzip2, readline, liburing, icu) this
  is "preparing aarch64 libs for further development".
- **`docker-bake.hcl`** — `CI_BASE_IMAGE_VERSION` → `pypa_2_28-pg18-arm-1`.

### Boost parity (1.88)
The buildroot toolchain now ships **Boost 1.88.0** (matching the x64 ci-base-image),
not buildroot 2025.02.4's default 1.83. Implemented via `boost_1_88_upgrade.patch`
(bump `package/boost/boost.mk` version + hash, drop the 1.83-era unordered patch) and
`BR2_PACKAGE_BOOST_RANDOM=y` in the config (1.88's b2 builds `libboost_random`
transitively; without selecting it buildroot's target-lib accounting aborts the build).

### Validation performed
- buildroot toolchain builds clean; aarch64 sysroot is consistent Boost 1.88 (header
  `108800`, 28 libs, no 1.83 remnants).
- **Relocatability:** copied `output/host` to a different path and compiled+ran a Boost
  program via the portable `Toolchain.cmake` (honors `$CROSS_ROOT`) → `boost 1_88`.
- **End-to-end:** cross-built `hived` from current develop (with develop's pinned fc
  `c3ed5c9`) against the Boost-1.88 toolchain → aarch64 ELF runs under qemu, reports the
  correct version.

### Note: fc and Boost 1.88's removed `to_ulong()`
Boost 1.87+ removed `boost::asio::ip::address_v4::to_ulong()` / `address_v6::to_v4()`
(use `to_uint()`). Develop's pinned fc (`c3ed5c9`) already uses `to_uint()` and builds
clean against Boost 1.88. A **divergent WIP fc branch** (`1f38335`, currently checked out
in this working tree) still calls the removed `to_ulong()` and will not compile against
Boost 1.88 on any arch — it needs the same `to_ulong()`→`to_uint()` migration before it
can use the 1.88 toolchain (or the bumped x64 ci-base-image).
