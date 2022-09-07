# Building Hive

Building Hive requires 8GB of RAM.

## Compile-Time Options (cmake)

### CMAKE_BUILD_TYPE=[Release/Debug]

Specifies whether to build with or without optimization and without or with
the symbol table for debugging. Unless you are specifically debugging or
running tests, it is recommended to build as release.

### BUILD_HIVE_TESTNET=[OFF/ON]

Builds hived for use in a private testnet. Also required for building unit tests.

## Building under Docker

We ship a Dockerfile.  This builds both common node type binaries.

    git clone https://github.com/openhive-network/hive
    cd hive
    docker build -t hiveio/hive --target=consensus_node .

## Building on Ubuntu 18.04/20.04

For Ubuntu 18.04/20.04 users, after installing the right packages with `apt` Hive
will build out of the box without further effort:

    sudo apt-get update

    # Required packages
    sudo apt-get install -y \
        autoconf \
        automake \
        cmake \
        g++ \
        git \
        zlib1g-dev \
        libbz2-dev \
        libsnappy-dev \
        libssl-dev \
        libtool \
        make \
        pkg-config \
        doxygen \
        libncurses5-dev \
        libreadline-dev \
        libboost-all-dev \
        perl \
        python3 \
        python3-jinja2

    git clone https://github.com/openhive-network/hive
    cd hive
    git checkout master
    git submodule update --init --recursive
    mkdir build && cd build
    cmake -DCMAKE_BUILD_TYPE=Release ..
    make -j$(nproc) hived
    make -j$(nproc) cli_wallet
    # optional
    make install  # defaults to /usr/local

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
