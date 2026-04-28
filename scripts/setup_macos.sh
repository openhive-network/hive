#! /bin/bash

set -euo pipefail

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"

# Installs the Homebrew packages required to build hived natively on macOS
# (Apple Silicon / Intel). This is a developer-convenience target: a native
# build is not a supported runtime — see scripts/setup_ubuntu.sh for the
# canonical production setup.
#
# After running this script, a typical build looks like:
#
#   export OPENSSL_ROOT_DIR="$(brew --prefix openssl@3)"
#   export BOOST_ROOT="$(brew --prefix boost)"
#   mkdir build && cd build
#   cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_HIVE_TESTNET=ON -GNinja ..
#   ninja hived cli_wallet beekeeper

print_help () {
cat <<EOF
Usage: $0 [OPTION]...

Install macOS dependencies for building hived natively.

OPTIONS:
  --dev       Install everything needed to build and test hived.
  --user      Install user-level Python tooling (poetry).
  --help      Display this help screen and exit.
EOF
}

assert_homebrew() {
  if ! command -v brew >/dev/null 2>&1; then
    echo "Homebrew is required. Install it from https://brew.sh and re-run." >&2
    exit 1
  fi
}

install_all_dev_packages() {
  echo "Installing build dependencies via Homebrew..."
  assert_homebrew

  brew update

  # Build toolchain + libraries.
  # boost: must be ≥ 1.74 per top-level CMakeLists.txt
  # openssl@3: macOS ships LibreSSL; we need real OpenSSL 3.x
  # readline: cli_wallet line-editing
  # snappy / lz4 / zstd / bzip2 / zlib: rocksdb compression backends
  # gperftools: optional tcmalloc (autodetected by hive_targets.cmake)
  # libpqxx: only needed for HAF-style downstream tools, harmless on a base build
  brew install \
    cmake ninja pkg-config autoconf automake libtool \
    boost openssl@3 readline \
    snappy lz4 zstd bzip2 zlib \
    gperftools libpqxx \
    zopfli \
    python@3.12 git

  # jinja2 is a build-time helper used by libraries/jsonball.
  # PEP 668 blocks plain pip3 on Homebrew Python; use --break-system-packages.
  pip3 install --user --break-system-packages jinja2

  cat <<EOF

Done. Add these to your shell before configuring the build:

  export OPENSSL_ROOT_DIR="\$(brew --prefix openssl@3)"
  export BOOST_ROOT="\$(brew --prefix boost)"

EOF
}

install_user_packages() {
  echo "Installing user-level Python tooling..."
  assert_homebrew

  # poetry is what tests/python/hive-local-tools uses.
  if ! command -v poetry >/dev/null 2>&1; then
    curl -sSL https://install.python-poetry.org | python3 -
  fi
  poetry self update 2.1.3 || true
  poetry self add "poetry-dynamic-versioning[plugin]@>=1.0.0,<2.2.0" || true
}

if [ $# -eq 0 ]; then
  print_help
  exit 0
fi

while [ $# -gt 0 ]; do
  case "$1" in
    --dev)
        install_all_dev_packages
        ;;
    --user)
        install_user_packages
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
