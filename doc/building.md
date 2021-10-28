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

If at any time you find this documentation not up to date or unprecise, please take a look at CI/CD scripts.

## Building on macOS X

Install Xcode and its command line tools by following the instructions here:
https://guide.macports.org/#installing.xcode.  In OS X 10.11 (El Capitan)
and newer, you will be prompted to install developer tools when running a
developer command in the terminal.

Accept the Xcode license if you have not already:

    sudo xcodebuild -license accept

Install Homebrew by following the instructions here: http://brew.sh/

### Initialize Homebrew:

    brew doctor
    brew update

### Install hive dependencies:

    brew install \
        autoconf \
        automake \
        cmake \
        git \
        boost160 \
        libtool \
        openssl \
        snappy \
        zlib \
        bzip2 \
        python3
        
    pip3 install --user jinja2
    
Note: brew recently updated to boost 1.61.0, which is not yet supported by
hive. Until then, this will allow you to install boost 1.60.0.
You may also need to install zlib and bzip2 libraries manually.
In that case, change the directories for `export` accordingly.

*Optional.* To use TCMalloc in LevelDB:

    brew install google-perftools

*Optional.* To use cli_wallet and override macOS's default readline installation:

    brew install --force readline
    brew link --force readline

### Clone the Repository

    git clone https://github.com/openhive-network/hive.git
    cd hive

### Compile

    export BOOST_ROOT=$(brew --prefix)/Cellar/boost@1.60/1.60.0/
    export OPENSSL_ROOT_DIR=$(brew --prefix)/Cellar/openssl/1.0.2q/
    export SNAPPY_ROOT_DIR=$(brew --prefix)/Cellar/snappy/1.1.7_1
    export ZLIB_ROOT_DIR=$(brew --prefix)/Cellar/zlib/1.2.11
    export BZIP2_ROOT_DIR=$(brew --prefix)/Cellar/bzip2/1.0.6_1
    git checkout stable
    git submodule update --init --recursive
    mkdir build && cd build
    cmake -DBOOST_ROOT="$BOOST_ROOT" -DCMAKE_BUILD_TYPE=Release ..
    make -j$(sysctl -n hw.logicalcpu)

Also, some useful build targets for `make` are:

    hived
    chain_test
    cli_wallet

e.g.:

    make -j$(sysctl -n hw.logicalcpu) hived

This will only build `hived`.

## Building on Other Platforms

- Windows build instructions do not yet exist.

- The developers normally compile with gcc and clang. These compilers should
  be well-supported.
- Community members occasionally attempt to compile the code with mingw,
  Intel and Microsoft compilers. These compilers may work, but the
  developers do not use them. Pull requests fixing warnings / errors from
  these compilers are accepted.
