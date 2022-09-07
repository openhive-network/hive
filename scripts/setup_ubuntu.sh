#! /bin/bash

set -euo pipefail 

# Script purpose is an installation of all packages required to build and run Hived instance.
# After changing it, please also update and push to the registry a docker image defined in https://gitlab.syncad.com/hive/hive/-/blob/develop/Dockerfile

#Updated docker image must be also explicitly referenced in the https://gitlab.syncad.com/hive/hive/-/blob/develop/.gitlab-ci.yml#L11


print_help () {
    echo "Usage: $0 [OPTION[=VALUE]]..."
    echo
    echo "Allows to setup this machine for Hived installation"
    echo "OPTIONS:"
    echo "  --hived-account=NAME      Allows to specify the account name to be used for hived process."
    echo "  --runtime                 Allows to install all packages required to run already built hived binary."
    echo "  --dev                     Allows to install all packages required to build and test hived project (additionally to package set specific to runtime)."
    echo "  --help                    Display this help screen and exit"
    echo
}

hived_unix_account="hived"

install_all_runtime_packages() {
  apt-get update && apt-get install -y language-pack-en && apt-get install -y sudo screen libsnappy1v5 libreadline8 wget && apt-get clean && rm -r /var/lib/apt/lists/*
}

install_all_dev_packages() {
  apt-get update && apt-get install -y \
  git python3 build-essential gir1.2-glib-2.0 libgirepository-1.0-1 libglib2.0-0 libglib2.0-data libxml2 python3-distutils python3-lib2to3 python3-pkg-resources shared-mime-info xdg-user-dirs ca-certificates \
  autoconf automake cmake clang clang-tidy g++ git libbz2-dev libsnappy-dev libssl-dev libtool make pkg-config python3-jinja2 libboost-all-dev doxygen libncurses5-dev libreadline-dev perl ninja-build \
  \
  screen python3-pip python3-dateutil tzdata python3-junit.xml python3-venv python3-dateutil && \
  \
  apt-get clean && rm -r /var/lib/apt/lists/* && \
  pip3 install -U secp256k1prp
}

while [ $# -gt 0 ]; do
  case "$1" in
    --runtime)
        install_all_runtime_packages
        ;;
    --dev)
        install_all_dev_packages
        ;;
    --hived-account=*)
        hived_unix_account="${1#*=}"
        ;;
    --help)
        print_help
        exit 0
        ;;
    -*)
        echo "ERROR: '$1' is not a valid option"
        echo
        print_help
        exit 1
        ;;
    *)
        echo "ERROR: '$1' is not a valid argument"
        echo
        print_help
        exit 2
        ;;
    esac
    shift
done

if [ "$EUID" -ne 0 ]
  then echo "Please run as root"
  exit 1
fi

if id "$hived_unix_account" &>/dev/null; then
    echo "Account $hived_unix_account already exists. Creation skipped."
else
    useradd -ms /bin/bash "$hived_unix_account" && echo "$hived_unix_account ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers
fi


