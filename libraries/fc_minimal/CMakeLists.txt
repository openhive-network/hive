

file(GLOB HEADERS "include/fc/*.hpp" )

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
        src/crypto/elliptic_secp256k1.cpp
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
        src/crypto/elliptic_impl_priv.cpp
        ${HEADERS}
     )

SET( BOOST_COMPONENTS )
# Here define all boost libraries being used by Hive subprojects
LIST( APPEND BOOST_COMPONENTS
        atomic
        date_time
        filesystem
        program_options
        system
)

set(BOOSTROOT "/home/mario/src/BOOST/sysroot/prebuilt_boost/")
set(Boost_USE_STATIC_RUNTIME ON)
#set(CMAKE_STATIC_LIBRARY_SUFFIX ".a;.bc")
set(Boost_DIR ${BOOSTROOT}/lib/cmake/Boost-1.82.0)
set(boost_atomic_DIR ${BOOSTROOT}/lib/cmake/boost_atomic-1.82.0)
set(boost_date_time_DIR ${BOOSTROOT}/lib/cmake/boost_date_time-1.82.0)
set(boost_filesystem_DIR ${BOOSTROOT}/lib/cmake/boost_filesystem-1.82.0)
set(boost_headers_DIR ${BOOSTROOT}/lib/cmake/boost_headers-1.82.0)
set(boost_program_options_DIR ${BOOSTROOT}/lib/cmake/boost_program_options-1.82.0)
set(boost_system_DIR ${BOOSTROOT}/lib/cmake/boost_system-1.82.0)

FIND_PACKAGE( Boost 1.82 REQUIRED CONFIG COMPONENTS ${BOOST_COMPONENTS} )
MESSAGE( "FOUND BOOST WITH COMPONENTS: ${BOOST_COMPONENTS}")

SET( binary_dir_secp256k1 "/home/mario/src/SEC256K1/secp256k1-zkp" )
add_library(secp256k1 STATIC IMPORTED)
set_property(TARGET secp256k1 PROPERTY IMPORTED_LOCATION ${binary_dir_secp256k1}/.libs/libsecp256k1.a)

SET( binary_dir_openssl "/home/mario/src/OPENSSL/openssl" )
SET( include_dir_openssl "/home/mario/src/OPENSSL/openssl/include" )
add_library(openssl_wasm STATIC IMPORTED)
set_property(TARGET openssl_wasm PROPERTY IMPORTED_LOCATION ${binary_dir_openssl}/libssl.a)

add_library(crypto_wasm STATIC IMPORTED)
set_property(TARGET crypto_wasm PROPERTY IMPORTED_LOCATION ${binary_dir_openssl}/libcrypto.a)

add_library ( fc_minimal STATIC ${fc_minimal_sources} )

SET( public_includes
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${Boost_INCLUDE_DIR}
        ${include_dir_openssl}
)

SET( private_includes
        ${binary_dir_secp256k1}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/simdjson
)

TARGET_INCLUDE_DIRECTORIES( fc_minimal PUBLIC ${public_includes} PRIVATE ${private_includes} )
target_link_libraries( fc_minimal PUBLIC ${Boost_LIBRARIES} openssl_wasm crypto_wasm secp256k1 )
