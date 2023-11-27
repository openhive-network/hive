#! /bin/bash

set -euo pipefail

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"

# This script installs all packages required to build and run a hived instance.
# After changing it, be sure to update and push to the registry a docker image defined in https://gitlab.syncad.com/hive/hive/-/blob/develop/Dockerfile

# The updated docker image must be also explicitly referenced in the https://gitlab.syncad.com/hive/hive/-/blob/develop/.gitlab-ci.yml#L11


print_help () {
    echo "Usage: $0 [OPTION[=VALUE]]..."
    echo
    echo "Setup this machine for hived installation"
    echo "OPTIONS:"
    echo "  --hived-admin-account=NAME  Specify the account name with sudo privileges."
    echo "  --hived-account=NAME        Specify the account name to be used for hived process."
    echo "  --runtime                   Installs all packages required to run a pre-built hived binary."
    echo "  --dev                       Installs all packages required to build and test hived project (in addition to the required runtime packages)."
    echo "  --user                      Installs all packages to the user's home directory)."
    echo "  --help                      Display this help screen and exit."
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

  #Additionally fix OpenSSL configuration issues caused by OpenSSL 3.0 
  # TODO REMOVE the additional openssl configuaration when OpenSSL 3.0.7 or above will be distributed by Ubuntu.
  cp "${SCRIPTPATH}/openssl.conf" /etc/ssl/hive-openssl.conf
  echo -e "\n.include /etc/ssl/hive-openssl.conf\n" >> /etc/ssl/openssl.cnf
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
  python3-dev p7zip-full \
  && \
  apt-get clean && rm -r /var/lib/apt/lists/* && \
  pip3 install -U secp256k1prp
}

preconfigure_faketime() {
  git clone --depth 1 --branch bw_timer_settime_fix https://gitlab.syncad.com/bwrona/faketime.git
  pushd faketime && CFLAGS="-O2 -DFAKE_STATELESS=1" make

  sudo make install # install it into default location path.

  popd
}

preconfigure_git_safe_direcories() {
  git config --global --add safe.directory '*'
}

install_user_packages() {
  base_dir=${1}
  echo "Attempting to install user packages in directory: ${base_dir}"

  mkdir -p "${base_dir}"
  pushd "${base_dir}"

  preconfigure_faketime
  preconfigure_git_safe_direcories

  # update path once it will be invalidated (hopefully not): https://github.com/vi/websocat/releases
  wget https://github.com/vi/websocat/releases/download/v1.11.0/websocat.x86_64-unknown-linux-musl
  chmod a+x ./websocat.x86_64-unknown-linux-musl

  sudo cp ./websocat.x86_64-unknown-linux-musl /usr/local/bin/

  popd

  curl -sSL https://install.python-poetry.org | python3 -  # install poetry in an isolated environment
  poetry self update 1.5.1
  poetry self add "poetry-dynamic-versioning[plugin]@>=1.0.0,<2.0.0"
}

create_hived_admin_account() {
  echo "Attempting to create $hived_admin_unix_account account..."
  assert_is_root

  # Unfortunetely hived_admin must be able to su as root, because it must be able to modify hived account uid
  if id "$hived_admin_unix_account" &>/dev/null; then
      echo "Account $hived_admin_unix_account already exists. Creation skipped."
  else
      useradd -ms /bin/bash -u 2000 -g users -c "Hived admin account" "$hived_admin_unix_account" && echo "$hived_admin_unix_account ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers
  fi
}

create_hived_account() {
  echo "Attempting to create $hived_unix_account account..."
  assert_is_root

  if id "$hived_unix_account" &>/dev/null; then
      echo "Account $hived_unix_account already exists. Creation skipped."
  else
      useradd -ms /bin/bash -u 2001 -g users -c "Hived daemon account" "$hived_unix_account"
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



