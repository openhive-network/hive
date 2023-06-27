# #
# # Defines fc library target.

PROJECT( fc_minimal )
CMAKE_MINIMUM_REQUIRED( VERSION 2.8.12 )

file(GLOB HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp" )

SET( ECC_LIB secp256k1 )
SET( ECC_REST src/crypto/elliptic_impl_priv.cpp )

set( fc_minimal_sources
        src/variant.cpp
        src/exception.cpp
        src/variant_object.cpp
        src/filesystem.cpp
        src/stacktrace.cpp
        src/time.cpp
        src/string.cpp
        src/shared_ptr.cpp
        src/utf8.cpp
        src/asio.cpp
        src/log/log_message.cpp
        src/log/logger.cpp
        src/log/logger_config.cpp
        src/log/appender.cpp
        src/log/file_appender.cpp
        src/log/console_appender.cpp
        src/crypto/elliptic_common.cpp
        src/crypto/elliptic_${ECC_IMPL}.cpp
        src/crypto/sha512.cpp
        src/crypto/sha256.cpp
        src/crypto/aes.cpp
        src/crypto/hex.cpp
        src/crypto/base64.cpp
        src/crypto/openssl.cpp
        src/crypto/ripemd160.cpp
        src/crypto/cripemd160.cpp
        src/crypto/_digest_common.cpp
        src/crypto/base58.cpp
        src/crypto/city.cpp
        src/crypto/crc.cpp
        src/io/json.cpp
        src/io/datastream.cpp
        src/io/sstream.cpp
        src/io/fstream.cpp
        src/io/iostream.cpp
        src/thread/thread.cpp
        src/thread/future.cpp
        src/thread/spin_yield_lock.cpp
        src/thread/task.cpp
        src/thread/spin_lock.cpp
        vendor/simdjson/simdjson.cpp
        src/compress/zlib.cpp
        ${ECC_REST}
     )

SET( SECP256K1_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/secp256k1-zkp" )
SET( SEC256K1_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/vendor/secp256k1-zkp-build )

include(ExternalProject)

ExternalProject_Add( project_secp256k1_minimal
PREFIX ${CMAKE_CURRENT_BINARY_DIR}/vendor/secp256k1-zkp
SOURCE_DIR ${SECP256K1_SRC_DIR}
BINARY_DIR ${SEC256K1_BINARY_DIR}
CONFIGURE_COMMAND ${SECP256K1_SRC_DIR}/configure --prefix=${CMAKE_CURRENT_BINARY_DIR}/vendor/secp256k1-zkp --with-bignum=no --enable-tests=no --enable-benchmark=no --enable-module-recovery --enable-experimental --enable-module-generator --enable-module-rangeproof
BUILD_COMMAND make
INSTALL_COMMAND true
BUILD_BYPRODUCTS ${SEC256K1_BINARY_DIR}/.libs/libsecp256k1.a ${SEC256K1_BINARY_DIR}/.libs/libsecp256k1.so
)

ExternalProject_Add_Step(project_secp256k1_minimal autogen
WORKING_DIRECTORY ${SECP256K1_SRC_DIR}
COMMAND ${SECP256K1_SRC_DIR}/autogen.sh
DEPENDERS configure
)

ExternalProject_Get_Property(project_secp256k1_minimal binary_dir)

add_library(secp256k1 STATIC IMPORTED)
set_property(TARGET secp256k1 PROPERTY IMPORTED_LOCATION ${binary_dir}/.libs/libsecp256k1${CMAKE_STATIC_LIBRARY_SUFFIX})
set_property(TARGET secp256k1 PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/vendor/secp256k1-zkp/include)
add_dependencies(secp256k1 project_secp256k1_minimal)

ADD_LIBRARY( STATIC_BOOST_minimal INTERFACE )
SET_TARGET_PROPERTIES( STATIC_BOOST_minimal PROPERTIES POSITION_INDEPENDENT_CODE OFF )
ADD_TARGET_BOOST_LIBRARIES( STATIC_BOOST_minimal )

ADD_LIBRARY ( STATIC_OPENSSL_minimal INTERFACE )
ADD_TARGET_OPENSSL_LIBRARIES (STATIC_OPENSSL_minimal TRUE)

add_library ( fc_minimal STATIC ${fc_minimal_sources} )

SET( public_includes
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${Boost_INCLUDE_DIR}
        ${OPENSSL_INCLUDE_DIR}
)

SET( private_includes
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/secp256k1-zkp
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/simdjson
)

TARGET_INCLUDE_DIRECTORIES( fc_minimal PUBLIC ${public_includes} PRIVATE ${private_includes} )
target_link_libraries( fc_minimal PUBLIC STATIC_BOOST_minimal STATIC_OPENSSL_minimal ${ECC_LIB} )