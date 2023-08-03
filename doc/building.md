# Building Hive

Building Hive requires at least 16GB of RAM. 

Hive project is described using CMake. By default it uses a Ninja tool as a build executor.

Only Linux based systems are supported as build and runtime platform. Nowadays Ubuntu 22.04 LTS is chosen as base OS supported by build and runtime processes. Build process requires tools available in default Ubuntu package repository.

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

## Building using Docker

We ship a Dockerfile.

    mkdir workdir
    cd workdir # use out-of-source directory to keep source directory clean
    ../hive/scripts/ci-helpers/build_instance.sh my-tag ../hive registry.gitlab.syncad.com/hive/hive

`build_instance.sh` has optional parameters:
- `--network-type` which allows to select network type supported by built binaries. It can take values:
    - mainnet (default)
    - mirrornet
    - testnet

- `--export-binaries=PATH` - allows to extract built binaries from created image

Above example call will create the image: `registry.gitlab.syncad.com/hive/hive/instance:instance-my-tag`

To run given image you can use a helper script: `../hive/scripts/run_hived_img.sh registry.gitlab.syncad.com/hive/hive/instance:instance-my-tag --name=hived-instance --data-dir=/home/hive/datadir --shared-file-dir=/home/hive/datadir`

## Building native binaries on Ubuntu 22.04 LTS

Please run this script, or based on its content, manually install the required packages.

    sudo ../hive/scripts/setup_ubuntu.sh --dev

    mkdir build
    
    ../hive/scripts/build.sh --source-dir=../hive/ --binary-dir=./build/ --cmake-arg="-DBUILD_HIVE_TESTNET=OFF"

The --cmake_arg parameter can be skipped, then default mainnet release binaries will be built.

If you would like to run cmake directly, you can do it as follows:

    cd build
    cmake -DCMAKE_BUILD_TYPE=Release -GNinja ../../hive/

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
