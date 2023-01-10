# Building Hive

Building Hive requires at least 16GB of RAM. 

Hive project is described using CMake. By default it uses a Ninja tool as a build executor.

Only Linux based systems are supported as build and runtime platform. Nowadays Ubuntu 20.04 LTS is chosen as base OS supported by build and runtime processes (Ubuntu 18.04 LTS is not supported anymore). Build process requires tools available in default Ubuntu package repository.

## Getting sources

To get Hive sources, please clone a git repository using following command line:

    git clone --recurse --branch master https://github.com/openhive-network/hive

## Compile-Time Options (cmake)

### CMAKE_BUILD_TYPE=[Release/RelWithDebInfo/Debug]

Specifies whether to build with or without optimization and without or with
the symbol table for debugging. Unless you are specifically debugging or
running tests, it is recommended to build as Release or at least RelWithDebInfo (which includes debugging symbols, but should not have significant impact on performance).

### BUILD_HIVE_TESTNET=[OFF/ON]

Builds Hive project for use in a private testnet. Also required for building unit tests.

### HIVE_CONVERTER_BUILD=[ON/OFF]

Builds Hive project in MirrorNet configuration (similar to testnet, but allows to import mainnet data to create better testing environemnt).

## Building under Docker

We ship a Dockerfile.  This builds both common node type binaries.

    mkdir workdir
    cd workdir # use different directory to leave source directory clean
    ../hive/scripts/ci-helpers/build_instance.sh my-local-tag ../hive/ registry.gitlab.syncad.com/hive/hive/

`build_instance.sh` has optional parameters:
- `--network-type` which allows to select network type supported by built binaries. It can take values:
    - testnet
    - mirrornet
    - mainnet (default)

- `--export-binaries=PATH` - allows to extract built binaries from created image
- `--cache-path=TAG-SUFFIX` - allows to specify a custom path to BuildKit cache image (requires BuildKit builder with docker-container driver), meant to be used with CI

Above example call will create the image: `registry.gitlab.syncad.com/hive/hive/instance:my-local-tag`

To run given image you can use a helper script: `../hive/scripts/run_hived_img.sh registry.gitlab.syncad.com/hive/hive/instance:my-local-tag --name=hived-instance --data-dir=../datadir --shared-file-dir../datadir/ #[<other regular hived options>]`

## Building on Ubuntu 20.04 LTS

For Ubuntu 20.04 LTS users, after installing the right packages with `apt` Hive should build out of the box without further effort:

    # workdir directory is assumed as cwd, like also already cloned repository using command line described earlier

    sudo ../hive/scripts/setup_ubuntu.sh --dev # this script contains a list of all required packages for runtime and development (build) process

    mkdir build
    
    ../hive/scripts/build.sh --source-dir=../hive/ --binary-dir=./build/ --cmake-arg="-DBUILD_HIVE_TESTNET=OFF"

The --cmake_arg parameter can be skipped, then default mainnet release binaries will be built.

If you would like to run cmake directly, you can do it as follows:

    cd build
    cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_HIVE_TESTNET=OFF -GNinja ../../hive/

To start build process:

    ninja

If you want to build only specific targets use:

    ninja hived cli_wallet

Ninja tool can be installed from standard Ubuntu package repositories by using:

    sudo apt-get install -y ninja-build

**If at any time you find this documentation not up to date or unprecise, please take a look at CI/CD scripts.**

## Building on Other Platforms
- macOS instructions were old and obsolete, feel free to contribute.
- Windows build instructions do not yet exist.
- The developers normally compile with gcc and clang. These compilers should
  be well-supported.
- Community members occasionally attempt to compile the code with mingw,
  Intel and Microsoft compilers. These compilers may work, but the
  developers do not use them. Pull requests fixing warnings / errors from
  these compilers are accepted.
