#! /bin/bash

set -euo pipefail

# Builds the shared libraries of the Boost components hive uses and installs them next to the static ones, for the
# shared-library variant (HIVE_BUILD_SHARED_LIBS) on a system whose Boost is static only, as the CI build image's is.
# Its static libraries cannot be put into a shared one instead: they are compiled with hidden visibility, so none of
# their symbols would be exported. The shared libraries are built from the source of the installed headers' version,
# which must be the version pinned below.

BOOST_VERSION="1.90.0"
BOOST_SHA256="49551aff3b22cbc5c5a9ed3dbc92f0e23ea50a0f7325b0d198b705e8ee3fc305"
# Boost.Test stays static (cmake/hive_targets.cmake)
BOOST_LIBRARIES="atomic,chrono,context,coroutine,date_time,filesystem,iostreams,program_options,regex,thread"

print_help () {
cat <<EOF
Usage: $0 [OPTION[=VALUE]]...

Builds Boost ${BOOST_VERSION}'s shared libraries and installs them next to its static ones
  --prefix=DIRECTORY_PATH  Boost's installation prefix, holding include/boost and lib (default: /usr/local)
  --help                   Display this help screen and exit
EOF
}

PREFIX="/usr/local"

while [ $# -gt 0 ]; do
  case "$1" in
    --prefix=*)
        PREFIX="${1#*=}"
        ;;
    --help)
        print_help
        exit 0
        ;;
    *)
        echo "ERROR: '$1' is not a valid option"
        echo
        print_help
        exit 1
        ;;
    esac
    shift
done

if [[ ! -f "$PREFIX/include/boost/version.hpp" ]]; then
  echo "ERROR: no Boost is installed in ${PREFIX}"
  exit 1
fi
version_number=$(sed -n 's/^#define BOOST_VERSION \([0-9]*\)$/\1/p' "$PREFIX/include/boost/version.hpp")
installed_version="$((version_number / 100000)).$((version_number / 100 % 1000)).$((version_number % 100))"
if [[ "$installed_version" != "$BOOST_VERSION" ]]; then
  echo "ERROR: Boost ${installed_version} is installed in ${PREFIX}, but this script builds ${BOOST_VERSION}; update its pin"
  exit 1
fi

work_dir=$(mktemp -d)
trap 'rm -rf "$work_dir"' EXIT

archive="boost_${BOOST_VERSION//./_}.tar.bz2"
curl -fsSL "https://archives.boost.io/release/${BOOST_VERSION}/source/${archive}" -o "$work_dir/$archive"
echo "${BOOST_SHA256}  $work_dir/$archive" | sha256sum -c -
tar -xjf "$work_dir/$archive" -C "$work_dir"

pushd "$work_dir/boost_${BOOST_VERSION//./_}" > /dev/null
./bootstrap.sh --with-libraries="$BOOST_LIBRARIES"
# Without ICU, which Boost.Regex would otherwise link where it is installed: hive does not use it. Compiled and linked
# with the flags of hive's own shared libraries (CMakeLists.txt).
./b2 -j "$(nproc)" -d0 --disable-icu link=shared variant=release threading=multi runtime-link=shared \
  cxxflags=-fno-semantic-interposition cflags=-fno-semantic-interposition linkflags=-Wl,-Bsymbolic-functions \
  stage --stagedir="$work_dir/stage"
popd > /dev/null

if [[ -w "$PREFIX/lib" ]]; then
  cp -a "$work_dir/stage/lib/"*.so* "$PREFIX/lib/"
else
  sudo cp -a "$work_dir/stage/lib/"*.so* "$PREFIX/lib/"
fi
ls -la "$PREFIX/lib/"libboost_*.so*
