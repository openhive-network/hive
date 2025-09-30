# Building hived (Hive blockchain P2P node)

Building a hived node requires at least 16GB of RAM. 

A hived node is built using CMake. 

By default, Ninja is used as the build executor. Ninja supports parallel compilation and by default will allow up to N simultaneous compiles where N is the number of CPU cores on your build system. 

If your build system has a lot of cores and not a lot of memory, you may need to explicitly limit the number of parallel build steps allowed (e.g `ninja -j4` to limit to 4 simultaneous compiles).

Only Linux-based systems are supported as a build and runtime platform. Currently Ubuntu 22.04 LTS is the minimum base OS supported by the build and runtime processes. The build process requires tools available in the default Ubuntu package repository.

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

## Building native binaries on Ubuntu 22.04 LTS

### Prerequisites

Run the script below, or based on its contents, manually install the required packages:

    sudo ../hive/scripts/setup_ubuntu.sh --dev

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
- Support for ARM:
  To target aarch64 architecture, we need to build cross toolchain operating on x64 host and building for aarch64 target. To do it let's use a buildroot in version `2025.02.4`.
  - get buildroot: `git clone https://github.com/buildroot/buildroot.git`
  - checkout given tag: `git checkout tags/2025.02.4`
  - copy ./aarch64/buildroot/.config into <src-root>/buildroot/.config
  - copy ./aarch64/buildroot/glibc_downgrade.patch into <src-root>/buildroot/glibc_downgrade.patch
  - copy ./aarch64/buildroot/snappy_static_build.patch <src-root>/buildroot/snappy_static_build.patch
  - go into <src-root>/buildroot/
  - Optionally execute make menuconfig to review configuration
  - execute make to build toolchain
  - copy ./aarch64/Toolchain.cmake to <src-root>/buildroot/output/host
  - pack built toolchain located in: <src-root>/buildroot/output/host for futher use
  - build hived using <src-root>/scripts/build_arm.sh script (specify actual toolchain path first as `CROSS_ROOT` variable in the script
  Hived was initially tested on Raspberry Pi 4B, 8GB RAM, 128GB SD-CARD storage using Raspian OS 64 bit, https://downloads.raspberrypi.com/raspios_full_arm64/images/raspios_full_arm64-2024-11-19/2024-11-19-raspios-bookworm-arm64-full.img.xz
  Preferred usage scenario is to generate a hived snapshot on x64 platform (using regular x64 build), then copy it into ARM machine and load to continue syncing to reach head block.
