# Building hived (Hive blockchain P2P node)

Building a hived node requires at least 16GB of RAM. 

A hived node is built using CMake. 

By default, Ninja is used as the build executor. Ninja supports parallel compilation and by default will allow up to N simultaneous compiles where N is the number of CPU cores on your build system. 

If your build system has a lot of cores and not a lot of memory, you may need to explicitly limit the number of parallel build steps allowed (e.g `ninja -j4` to limit to 4 simultaneous compiles).

Only Linux-based systems are supported as a build and runtime platform. Currently Ubuntu 24.04 LTS is the minimum base OS supported by the build and runtime processes. The build process requires tools available in the default Ubuntu package repository.

## Getting hive source code

To get the source code, clone a git repository using the following command line:

    git clone --recurse --branch master https://github.com/openhive-network/hive

## Compile-Time Options (cmake options)

### CMAKE_BUILD_TYPE=[Release/RelWithDebInfo/Debug]

Specifies whether to build with or without optimizations and without or with
the symbol table for debugging. Unless you are specifically debugging or
running tests, it is recommended to build as Release or at least RelWithDebInfo (which includes debugging symbols, but does not have a significant impact on performance).

### BUILD_HIVE_TESTNET=[OFF/ON]

Builds hived for use in a private testnet. Also required for building unit tests.

### HIVE_CONVERTER_BUILD=[ON/OFF]

Builds Hive project in MirrorNet configuration (similar to testnet, but enables importing mainnet data to create a better testing environment).

## Building hived as a docker image

We ship a Dockerfile.

    mkdir workdir
    cd workdir # use an out-of-source build directory to keep the source directory clean
    ../hive/scripts/ci-helpers/build_instance.sh my-tag ../hive registry.gitlab.syncad.com/hive/hive

`build_instance.sh` has optional parameters:
- `--network-type` specifies the type of P2P network supported by the hived node being built. Allowed values are:
    - mainnet (default)
    - mirrornet
    - testnet

- `--export-binaries=PATH` - extracts the built binaries from the created docker image

The example command above will build an image named `registry.gitlab.syncad.com/hive/hive:my-tag`

To run the given image, you can use a helper script:

    ../hive/scripts/run_hived_img.sh registry.gitlab.syncad.com/hive/hive:my-tag --name=hived-instance --data-dir=/home/hive/datadir --shared-file-dir=/home/hive/datadir

## Building native binaries on Ubuntu 24.04 LTS

### Prerequisites

Run the script below, or based on its contents, manually install the required packages:

    sudo hive/scripts/setup_ubuntu.sh --dev

### Configure cmake

    mkdir build
    cd build
    cmake -DCMAKE_BUILD_TYPE=Release -GNinja ../hive

### Build with Ninja

To start the build process, simply run:

    ninja

Or if you want to build only specific binary targets use:

    ninja hived cli_wallet

**If at any time you find this documentation not up-to-date or imprecise, please take a look at the CI/CD scripts in the scripts/ci-helpers directory.**

## Building on Other Platforms
- macOS instructions are old and obsolete, feel free to contribute.
- Windows build instructions do not exist yet.
- The developers normally compile with gcc and clang. These compilers should
  be well-supported.
- Community members occasionally attempt to compile the code with mingw,
  Intel and Microsoft compilers. These compilers may work, but the
  developers do not use them. Pull requests fixing warnings / errors from
  these compilers are accepted.
- Support for ARM (aarch64): see the dedicated section below.

## Building for ARM (aarch64)

aarch64 `hived` is produced by **cross-compiling on an x86_64 host** — the heavy C++ compile is always run natively and is **never emulated under QEMU** (full emulation of a Boost + RocksDB + chain build is many hours and memory-heavy, so it is not used).

The cross toolchain (buildroot `2025.02.4`: GCC 14.2.0 / binutils 2.43.1 / glibc 2.35, plus aarch64 builds of Boost 1.88, OpenSSL, snappy, zlib, bzip2, readline, liburing, icu) **and** a preinstalled aarch64 RocksDB are **baked into the `ci-base-image`**. As long as you use that image you do **not** need to build the toolchain yourself.

The authoritative, CI-tested procedure is the `build` stage of the [Dockerfile](../Dockerfile) (its `TARGETARCH=arm64` branch). Everything below mirrors it.

### What the ci-base-image provides

The arm-enabled `ci-base-image` exports these paths (used by every cross build):

| Variable | Value | Purpose |
|----------|-------|---------|
| `CROSS_ROOT` | `/opt/hive/cross/aarch64` | toolchain root; portable `Toolchain.cmake` lives at `$CROSS_ROOT/Toolchain.cmake` |
| `HIVE_AARCH64_VENDOR_DIR` | `/opt/hive/vendor-aarch64` | preinstalled aarch64 RocksDB, so the ~2000-file RocksDB build is skipped |

The exact image reference is pinned in [Dockerfile](../Dockerfile) (`ARG CI_BASE_IMAGE`) and [scripts/ci-helpers/ci_image_tag_vars.yml](../scripts/ci-helpers/ci_image_tag_vars.yml) (`CI_BASE_IMAGE_TAG`); use that value as `<ci-base-image>` below.

### Option A — Build the aarch64 / multiarch image (recommended; identical to CI)

This drives the tested Dockerfile end to end. QEMU/binfmt is needed only for the small, apt-based arm64 *runtime* layer; the compile itself is cross (native speed).

```bash
# register binfmt so the emulated arm64 runtime stage can run
docker run --rm --privileged tonistiigi/binfmt --install arm64

mkdir workdir && cd workdir
../hive/scripts/ci-helpers/build_instance.sh my-tag ../hive <registry> \
  --network-type=mainnet --platforms="linux/amd64,linux/arm64"
```

Notes:
- A multi-arch manifest **cannot be `--load`ed** into the local docker daemon, so `build_instance.sh` **pushes** it when `--platforms` is given. Point `<registry>` at one you can push to (for purely local use, run a `registry:2` container and push there). `--platforms="linux/arm64"` builds aarch64 only.
- To run the image, pull it on an arm64 host (or under emulation). To extract the binaries instead of running a container, use Option B.

### Option B — Cross-compile directly inside the ci-base-image (fast iteration; produces aarch64 binaries)

Use this for a quick edit/compile loop or to obtain raw aarch64 binaries without docker buildx. Your local `hive` checkout must already have its submodules initialised (`git clone --recurse ...`). Start an interactive container of the **arm-enabled ci-base-image** with the source mounted read-write (the secp256k1 autotools sub-build writes generated files into the tree):

```bash
docker run --rm -it -v "$PWD/hive:/home/hived/source" <ci-base-image> bash
```

Then, inside the container, a single `--cross-root` flag drives the whole cross build. The toolchain (`$CROSS_ROOT`) and preinstalled aarch64 RocksDB (`$HIVE_AARCH64_VENDOR_DIR`) are already exported by the image, so no other environment setup is needed:

```bash
cd /home/hived
./source/scripts/build.sh --source-dir=./source --binary-dir=./build \
  --cross-root="$CROSS_ROOT" \
  --flat-binary-directory=/home/hived/bin --clean-after-build
```

`--cross-root` handles everything that previously had to be set by hand — the toolchain compilers, OpenSSL lookup, the qemu-aarch64 emulator, static readline/ncurses, the CMake toolchain file, and (via `$HIVE_AARCH64_VENDOR_DIR`) the preinstalled RocksDB. sccache is left disabled for cross builds automatically (the top-level `CMakeLists.txt` skips it when `CMAKE_CROSSCOMPILING` is set, because wrapping the buildroot cross-gcc breaks `-MF` dependency-file generation).

The aarch64 binaries land in `/home/hived/bin`. Confirm the target architecture with `file /home/hived/bin/hived` (expect `ELF 64-bit LSB ... ARM aarch64`).

> `--cross-root` defaults the vendor libs to `$HIVE_AARCH64_VENDOR_DIR`; pass `--vendor-dir=PATH` to override, or omit it (build RocksDB from source) when cross-compiling against a self-built toolchain that has no preinstalled vendor libs.

### Running the aarch64 build

- aarch64 `hived` links buildroot glibc 2.35; `ubuntu:24.04` arm64 ships glibc 2.39 (forward-compatible), so the minimal runtime satisfies it.
- Initially tested on a Raspberry Pi 4B (8GB RAM, 128GB SD card) running 64-bit Raspberry Pi OS (https://downloads.raspberrypi.com/raspios_full_arm64/images/raspios_full_arm64-2024-11-19/2024-11-19-raspios-bookworm-arm64-full.img.xz).
- Preferred usage scenario: generate a `hived` snapshot on an x86_64 node (regular x64 build), copy it to the ARM machine, and load it there to continue syncing to the head block.

### Appendix — building the cross toolchain yourself

You only need this to (re)generate the toolchain that gets baked into `ci-base-image` — e.g. when bumping its versions. For normal aarch64 builds use the prebuilt image (Options A/B). The buildroot configuration and patches live in [doc/aarch64/buildroot](aarch64/buildroot).

1. `git clone https://github.com/buildroot/buildroot.git && cd buildroot && git checkout tags/2025.02.4`
2. Install buildroot host prerequisites (not in `setup_ubuntu.sh`): `cpio unzip bc rsync file wget`.
3. Copy `doc/aarch64/buildroot/.config` to `buildroot/.config`.
4. **Apply** (do not just copy) the patches — they modify buildroot's own packages:
   `git apply /path/to/doc/aarch64/buildroot/glibc_downgrade.patch /path/to/doc/aarch64/buildroot/snappy_static_build.patch`
   (the `boost_1_88_upgrade.patch` used by the image build lives in `hive/common-ci-configuration`.)
5. Optionally `make menuconfig` to review, then `make` to build the toolchain into `output/host`.
6. Pack `output/host` for reuse, ensuring it contains a `Toolchain.cmake` that honours `$CROSS_ROOT` (the `ci-base-image` ships such a portable one; the copy in [doc/aarch64/Toolchain.cmake](aarch64/Toolchain.cmake) is a legacy variant with a hardcoded path).
7. Build against your self-built toolchain with the same flag as Option B — just point `--cross-root` at the packed path and omit the vendor libs so RocksDB is built from source: `scripts/build.sh --cross-root=<path-to>/output/host ...`. You will need `qemu-user-static` installed (it provides `qemu-aarch64-static`); `--cross-root` provisions it automatically when missing. (The older [scripts/build_arm.sh](../scripts/build_arm.sh) wrapper predates the `--cross-root` flag and is kept only for reference.)
