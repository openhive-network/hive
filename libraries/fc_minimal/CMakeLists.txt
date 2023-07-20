SET( fc_dir "../fc/")

SET( ECC_IMPL secp256k1 CACHE STRING "secp256k1 or openssl or mixed" )

IF( ECC_IMPL STREQUAL openssl )
  SET( ECC_REST ${fc_dir}/src/crypto/elliptic_impl_pub.cpp )
ELSE( ECC_IMPL STREQUAL openssl )
  SET( ECC_LIB secp256k1 )
  IF( ECC_IMPL STREQUAL mixed )
    SET( ECC_REST ${fc_dir}/src/crypto/elliptic_impl_priv.cpp ${fc_dir}/src/crypto/elliptic_impl_pub.cpp )
  ELSE( ECC_IMPL STREQUAL mixed )
    SET( ECC_REST ${fc_dir}/src/crypto/elliptic_impl_priv.cpp )
  ENDIF( ECC_IMPL STREQUAL mixed )
ENDIF( ECC_IMPL STREQUAL openssl )

file(GLOB fc_minimal_headers
        ${fc_dir}/include/fc/aligned.hpp
        ${fc_dir}/include/fc/array.hpp
        ${fc_dir}/include/fc/bitutil.hpp
        ${fc_dir}/include/fc/compress/zlib.hpp
        ${fc_dir}/include/fc/container/deque_fwd.hpp
        ${fc_dir}/include/fc/container/flat.hpp
        ${fc_dir}/include/fc/container/flat_fwd.hpp
        ${fc_dir}/include/fc/crypto/aes.hpp
        ${fc_dir}/include/fc/crypto/base58.hpp
        ${fc_dir}/include/fc/crypto/base64.hpp
        ${fc_dir}/include/fc/crypto/bigint.hpp
        ${fc_dir}/include/fc/crypto/city.hpp
        ${fc_dir}/include/fc/crypto/cripemd160.hpp
        ${fc_dir}/include/fc/crypto/equihash.hpp
        ${fc_dir}/include/fc/crypto/hex.hpp
        ${fc_dir}/include/fc/crypto/hmac.hpp
        ${fc_dir}/include/fc/crypto/openssl.hpp
        ${fc_dir}/include/fc/crypto/ripemd160.hpp
        ${fc_dir}/include/fc/crypto/sha1.hpp
        ${fc_dir}/include/fc/crypto/sha224.hpp
        ${fc_dir}/include/fc/crypto/sha256.hpp
        ${fc_dir}/include/fc/crypto/sha512.hpp
        ${fc_dir}/include/fc/exception/exception.hpp
        ${fc_dir}/include/fc/filesystem.hpp
        ${fc_dir}/include/fc/fwd.hpp
        ${fc_dir}/include/fc/fwd_impl.hpp
        ${fc_dir}/include/fc/int_array.hpp
        ${fc_dir}/include/fc/interprocess/container.hpp
        ${fc_dir}/include/fc/io/buffered_iostream.hpp
        ${fc_dir}/include/fc/io/datastream.hpp
        ${fc_dir}/include/fc/io/enum_type.hpp
        ${fc_dir}/include/fc/io/fstream.hpp
        ${fc_dir}/include/fc/io/iostream.hpp
        ${fc_dir}/include/fc/io/json.hpp
        ${fc_dir}/include/fc/io/json_relaxed.hpp
        ${fc_dir}/include/fc/io/raw.hpp
        ${fc_dir}/include/fc/io/raw_fwd.hpp
        ${fc_dir}/include/fc/io/raw_variant.hpp
        ${fc_dir}/include/fc/io/sstream.hpp
        ${fc_dir}/include/fc/io/stdio.hpp
        ${fc_dir}/include/fc/io/varint.hpp
        ${fc_dir}/include/fc/log/appender.hpp
        ${fc_dir}/include/fc/log/console_appender.hpp
        ${fc_dir}/include/fc/log/log_message.hpp
        ${fc_dir}/include/fc/log/logger.hpp
        ${fc_dir}/include/fc/log/logger_config.hpp
        ${fc_dir}/include/fc/macros.hpp
        ${fc_dir}/include/fc/optional.hpp
        ${fc_dir}/include/fc/platform_independence.hpp
        ${fc_dir}/include/fc/reflect/reflect.hpp
        ${fc_dir}/include/fc/reflect/typename.hpp
        ${fc_dir}/include/fc/reflect/variant.hpp
        ${fc_dir}/include/fc/safe.hpp
        ${fc_dir}/include/fc/shared_ptr.hpp
        ${fc_dir}/include/fc/smart_ref_fwd.hpp
        ${fc_dir}/include/fc/smart_ref_impl.hpp
        ${fc_dir}/include/fc/stacktrace.hpp
        ${fc_dir}/include/fc/static_variant.hpp
        ${fc_dir}/include/fc/string.hpp
        ${fc_dir}/include/fc/thread/scoped_lock.hpp
        ${fc_dir}/include/fc/thread/spin_lock.hpp
        ${fc_dir}/include/fc/thread/spin_yield_lock.hpp
        ${fc_dir}/include/fc/thread/unique_lock.hpp
        ${fc_dir}/include/fc/time.hpp
        ${fc_dir}/include/fc/uint128.hpp
        ${fc_dir}/include/fc/unique_ptr.hpp
        ${fc_dir}/include/fc/utf8.hpp
        ${fc_dir}/include/fc/utility.hpp
        ${fc_dir}/include/fc/variant.hpp
        ${fc_dir}/include/fc/variant_object.hpp
        ${fc_dir}/include/fc/vector.hpp
        ${fc_dir}/src/crypto/_digest_common.hpp
        ${fc_dir}/src/log/console_defines.h
        ${fc_dir}/src/utf8/checked.h
        ${fc_dir}/src/utf8/core.h
        ${fc_dir}/src/utf8/unchecked.h
        ${fc_dir}/vendor/simdjson/simdjson.h
        )

set( fc_minimal_sources
        ${fc_dir}/src/variant.cpp
        ${fc_dir}/src/exception.cpp
        ${fc_dir}/src/variant_object.cpp
        ${fc_dir}/src/filesystem.cpp
        ${fc_dir}/src/stacktrace.cpp
        ${fc_dir}/src/time.cpp
        ${fc_dir}/src/string.cpp
        ${fc_dir}/src/shared_ptr.cpp
        ${fc_dir}/src/utf8.cpp
        ${fc_dir}/src/log/log_message.cpp
        ${fc_dir}/src/log/logger.cpp
        ${fc_dir}/src/log/logger_config.cpp
        ${fc_dir}/src/log/appender.cpp
        ${fc_dir}/src/log/console_appender.cpp
        ${fc_dir}/src/crypto/elliptic_common.cpp
        ${ECC_REST}
        ${fc_dir}src/crypto/elliptic_${ECC_IMPL}.cpp
        ${fc_dir}/src/crypto/sha512.cpp
        ${fc_dir}/src/crypto/sha256.cpp
        ${fc_dir}/src/crypto/aes.cpp
        ${fc_dir}/src/crypto/hex.cpp
        ${fc_dir}/src/crypto/base64.cpp
        ${fc_dir}/src/crypto/openssl.cpp
        ${fc_dir}/src/crypto/ripemd160.cpp
        ${fc_dir}/src/crypto/cripemd160.cpp
        ${fc_dir}/src/crypto/_digest_common.cpp
        ${fc_dir}/src/crypto/base58.cpp
        ${fc_dir}/src/crypto/city.cpp
        ${fc_dir}/src/crypto/crc.cpp
        ${fc_dir}/src/io/json.cpp
        ${fc_dir}/src/io/datastream.cpp
        ${fc_dir}/src/io/sstream.cpp
        ${fc_dir}/src/io/fstream.cpp
        ${fc_dir}/src/io/iostream.cpp
        ${fc_dir}/src/thread/spin_yield_lock.cpp
        ${fc_dir}/src/thread/spin_lock.cpp
        ${fc_dir}/vendor/simdjson/simdjson.cpp
        ${fc_dir}/src/compress/zlib.cpp
        ${fc_minimal_headers}
     )

SET( BOOST_COMPONENTS )
# Here define all boost libraries being used by Hive subprojects
LIST( APPEND BOOST_COMPONENTS
        atomic
        chrono
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
set(boost_chrono_DIR ${BOOSTROOT}/lib/cmake/boost_chrono-1.82.0)
set(boost_date_time_DIR ${BOOSTROOT}/lib/cmake/boost_date_time-1.82.0)
set(boost_filesystem_DIR ${BOOSTROOT}/lib/cmake/boost_filesystem-1.82.0)
set(boost_headers_DIR ${BOOSTROOT}/lib/cmake/boost_headers-1.82.0)
set(boost_program_options_DIR ${BOOSTROOT}/lib/cmake/boost_program_options-1.82.0)
set(boost_system_DIR ${BOOSTROOT}/lib/cmake/boost_system-1.82.0)

FIND_PACKAGE( Boost 1.82 REQUIRED CONFIG COMPONENTS ${BOOST_COMPONENTS} )
MESSAGE( "FOUND BOOST WITH COMPONENTS: ${BOOST_COMPONENTS}")

SET( binary_dir_secp256k1 "/home/mario/src/SEC256K1-2/secp256k1-zkp" )
add_library(secp256k1 STATIC IMPORTED)
set_property(TARGET secp256k1 PROPERTY IMPORTED_LOCATION ${binary_dir_secp256k1}/.libs/libsecp256k1.a)

SET( binary_dir_openssl "/home/mario/src/OPENSSL3/openssl" )
SET( include_dir_openssl "/home/mario/src/OPENSSL3/openssl/include" )
add_library(openssl_wasm STATIC IMPORTED)
set_property(TARGET openssl_wasm PROPERTY IMPORTED_LOCATION ${binary_dir_openssl}/libssl.a)

add_library(crypto_wasm STATIC IMPORTED)
set_property(TARGET crypto_wasm PROPERTY IMPORTED_LOCATION ${binary_dir_openssl}/libcrypto.a)

add_library ( fc_minimal STATIC ${fc_minimal_sources} )

SET( public_includes
        ${CMAKE_CURRENT_SOURCE_DIR}/${fc_dir}/include
        ${Boost_INCLUDE_DIR}
        ${include_dir_openssl}
)

SET( private_includes
        ${binary_dir_secp256k1}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/${fc_dir}/vendor/simdjson
)

TARGET_INCLUDE_DIRECTORIES( fc_minimal PUBLIC ${public_includes} PRIVATE ${private_includes} )
target_link_libraries( fc_minimal PUBLIC ${Boost_LIBRARIES} openssl_wasm crypto_wasm secp256k1 )
