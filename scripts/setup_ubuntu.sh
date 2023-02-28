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
    echo "  --hived-admin-account=NAME  Allows to specify the account name with sudo privileges."
    echo "  --hived-account=NAME        Allows to specify the account name to be used for hived process."
    echo "  --runtime                   Allows to install all packages required to run already built hived binary."
    echo "  --dev                       Allows to install all packages required to build and test hived project (additionally to package set specific to runtime)."
    echo "  --user                      Allows to install all packages being stored in the user's home directory)."
    echo "  --help                      Display this help screen and exit"
    echo
}

hived_admin_unix_account="hived_admin"
hived_unix_account="hived"

assert_is_root() {
  if [ "$EUID" -ne 0 ]
    then echo "Please run as root"
    exit 1
  fi
}

install_all_runtime_packages() {
  echo "Attempting to install all runtime packages..."
  assert_is_root

  apt-get update && apt-get install -y language-pack-en && apt-get install -y sudo screen libsnappy1v5 libreadline8 wget && apt-get clean && rm -r /var/lib/apt/lists/*
}

install_all_dev_packages() {
  echo "Attempting to install all dev packages..."
  assert_is_root

  apt-get update && apt-get install -y \
  git python3 build-essential gir1.2-glib-2.0 libgirepository-1.0-1 libglib2.0-0 libglib2.0-data libxml2 python3-distutils python3-lib2to3 python3-pkg-resources shared-mime-info xdg-user-dirs ca-certificates \
  autoconf automake cmake clang clang-tidy g++ git libbz2-dev libsnappy-dev libssl-dev libtool make pkg-config python3-jinja2 libboost-all-dev doxygen libncurses5-dev libreadline-dev perl ninja-build \
  xxd liburing-dev \
  \
  screen python3-pip python3-dateutil tzdata python3-junit.xml python3-venv python3-dateutil \
  python3-dev \
  && \
  apt-get clean && rm -r /var/lib/apt/lists/* && \
  pip3 install -U secp256k1prp
}

preconfigure_faketime() {
  git clone --depth 1 --branch v0.9.10 https://github.com/wolfcw/libfaketime.git
  pushd libfaketime && make

  sudo make install # install it into default location path.

  popd
}

install_user_packages() {
  base_dir=${1}
  echo "Attempting to install user packages in directory: ${base_dir}"

  mkdir -p "${base_dir}"
  pushd "${base_dir}"

  preconfigure_faketime

  # update path once it will be invalidated (hopefully not): https://github.com/vi/websocat/releases
  wget https://github.com/vi/websocat/releases/download/v1.11.0/websocat.x86_64-unknown-linux-musl
  chmod a+x ./websocat.x86_64-unknown-linux-musl

  sudo cp ./websocat.x86_64-unknown-linux-musl /usr/local/bin/

  popd

  curl -sSL https://install.python-poetry.org | python3 -  # install poetry in an isolated environment
}

create_hived_admin_account() {
  echo "Attempting to create $hived_admin_unix_account account..."
  assert_is_root

  # Unfortunetely hived_admin must be able to su as root, because it must be able to modify hived account uid
  if id "$hived_admin_unix_account" &>/dev/null; then
      echo "Account $hived_admin_unix_account already exists. Creation skipped."
  else
      useradd -ms /bin/bash -g users "$hived_admin_unix_account" && echo "$hived_admin_unix_account ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers
  fi
}

create_hived_account() {
  echo "Attempting to create $hived_unix_account account..."
  assert_is_root

  if id "$hived_unix_account" &>/dev/null; then
      echo "Account $hived_unix_account already exists. Creation skipped."
  else
      useradd -ms /bin/bash -g users "$hived_unix_account"
  fi
}

while [ $# -gt 0 ]; do
  case "$1" in
    --runtime)
        install_all_runtime_packages
        ;;
    --dev)
        install_all_dev_packages
        ;;
    --user)
        install_user_packages "${HOME}/hive_base_config"
        ;;
    --hived-admin-account=*)
        hived_admin_unix_account="${1#*=}"
        create_hived_admin_account
        ;;
    --hived-account=*)
        hived_unix_account="${1#*=}"
        create_hived_account
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



