=====================================How to compile libraries and beekeeper in WASM?=====================================

Install emscripten. (https://emscripten.org/)

**********BOOST**********
git clone https://github.com/boostorg/boost.git
cd boost
git checkout tags/boost-1.82.0
git submodule update --init --recursive
cd ..

export BOOST_DIR = here is a directory, where boost was saved

cd ${BOOST_DIR}/boost/tools/build
git checkout .

git apply emscripten_toolset.patch

mkdir -vp ${BOOST_DIR}/sysroot/prebuilt_boost
mkdir -vp ${BOOST_DIR}/sysroot/boost_build/

cd ${BOOST_DIR}/boost

./bootstrap.sh --without-icu --with-libraries=atomic,chrono,date_time,filesystem,program_options,system --prefix=${BOOST_DIR}/sysroot/prebuilt_boost/
./b2 --reconfigure toolset=emscripten link=static runtime-link=static --build-dir=${BOOST_DIR}/sysroot/boost_build/ --prefix=${BOOST_DIR}/sysroot/prebuilt_boost/ \
  host-os=linux target-os=linux \
  install

**********OPENSSL**********
git clone https://github.com/openssl/openssl.git
cd openssl/
git checkout tags/openssl-3.1.1 -b 3.1.1
git submodule update --init --recursive
emconfigure ./Configure no-hw no-shared no-asm no-threads no-ssl3 no-dtls no-engine no-dso linux-x32 -static
sed -i 's/$(CROSS_COMPILE)//' Makefile
emmake make -j 12 build_generated libssl.a libcrypto.a

**********SECP256**********
git clone https://github.com/ElementsProject/secp256k1-zkp.git
cd secp256k1-zkp/
./autogen.sh
#Important. If above script fails, then try to install: `sudo apt-get install autoconf` and/or sudo `apt-get install libtool`
emconfigure ./configure --with-asm=no --enable-shared=no --enable-tests=no --enable-benchmark=no --enable-exhaustive-tests=no --with-pic=no --with-valgrind=no --enable-module-recovery=yes
emmake make

**********beekeeper**********
HIVE_DIR = a directory where hived was saved
Libraries created in previous steps should be put into ${HIVE_DIR}/libraries/fc_minimal/CMakeLists.txt how it's showed (temporary paths are used).

cd ${HIVE_DIR}/programs/beekeeper/beekeeper_wasm/
mkdir build
cd build
cmake -DCMAKE_TOOLCHAIN_FILE=${PATH TO EMSCRIPTEN}/Emscripten.cmake -DCMAKE_BUILD_TYPE=Release ..
make -j12

=====================================How to test beekeeper in webbrowser?=====================================
Here was used HTTP server `serve`.

cd ${HIVE_DIR}/programs/beekeeper/beekeeper_wasm
Launch HTTP server: serve
Launch webbrowser with address: http://localhost:3000/beekeeper_wasm_test/beekeeper_wasm_test
For firefox right click and choose in menu `Inspect` and then choose `Console` tab.
