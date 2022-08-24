#
# Defines fc library target.

PROJECT( fc )
CMAKE_MINIMUM_REQUIRED( VERSION 2.8.12 )

MESSAGE(STATUS "Configuring project fc located in: ${CMAKE_CURRENT_SOURCE_DIR}")
SET( CMAKE_AUTOMOC OFF )

# Setup module path to make visible used CMake extensions
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/GitVersionGen")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules")

INCLUDE(GetPrerequisites)
INCLUDE( VersionMacros )
INCLUDE( SetupTargetMacros )
INCLUDE(GetGitRevisionDescription)

get_git_head_revision(GIT_REFSPEC FC_GIT_REVISION_SHA)
get_git_unix_timestamp(FC_GIT_REVISION_UNIX_TIMESTAMP)

SET( DEFAULT_HEADER_INSTALL_DIR include/\${target} )
SET( DEFAULT_LIBRARY_INSTALL_DIR lib/ )
SET( DEFAULT_EXECUTABLE_INSTALL_DIR bin/ )
SET( CMAKE_DEBUG_POSTFIX _debug )
SET( BUILD_SHARED_LIBS NO )
SET( ECC_IMPL secp256k1 CACHE STRING "secp256k1 or openssl or mixed" )

set(platformBitness 32)
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(platformBitness 64)
endif()

cmake_policy(SET CMP0054 NEW)

IF( ECC_IMPL STREQUAL openssl )
  SET( ECC_REST src/crypto/elliptic_impl_pub.cpp )
ELSE( ECC_IMPL STREQUAL openssl )
  SET( ECC_LIB secp256k1 )
  IF( ECC_IMPL STREQUAL mixed )
    SET( ECC_REST src/crypto/elliptic_impl_priv.cpp src/crypto/elliptic_impl_pub.cpp )
  ELSE( ECC_IMPL STREQUAL mixed )
    SET( ECC_REST src/crypto/elliptic_impl_priv.cpp )
  ENDIF( ECC_IMPL STREQUAL mixed )
ENDIF( ECC_IMPL STREQUAL openssl )

SET( SECP256K1_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/secp256k1-zkp" )
SET( SEC256K1_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/vendor/secp256k1-zkp-build )

# Configure secp256k1-zkp
if ( MSVC )
    # autoconf won't work here, hard code the defines
    set( SECP256K1_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/secp256k1-zkp" )

    file( GLOB SECP256K1_SOURCES "${SECP256K1_DIR}/src/secp256k1.c" )
    add_library( secp256k1 ${SECP256K1_SOURCES} )

    target_include_directories( secp256k1 PRIVATE "${SECP256K1_DIR}" PUBLIC "${SECP256K1_DIR}/include" )

    set( SECP256K1_BUILD_DEFINES
        USE_FIELD_10X26
        USE_FIELD_INV_BUILTIN
        USE_NUM_NONE
        USE_SCALAR_8X32
        USE_SCALAR_INV_BUILTIN )
    set_target_properties( secp256k1 PROPERTIES COMPILE_DEFINITIONS "${SECP256K1_BUILD_DEFINES}" LINKER_LANGUAGE C )
else ( MSVC )
    if( ${CMAKE_POSITION_INDEPENDENT_CODE} )
      SET( FPIC_FLAG "-fPIC" )
    endif()
    include(ExternalProject)
    if ( MINGW )
    ExternalProject_Add( project_secp256k1
     PREFIX ${CMAKE_CURRENT_BINARY_DIR}/vendor/secp256k1-zkp
     SOURCE_DIR ${SECP256K1_SRC_DIR}
     BINARY_DIR ${SEC256K1_BINARY_DIR}
     CONFIGURE_COMMAND ${SECP256K1_SRC_DIR}/configure CFLAGS=${FPIC_FLAG} --prefix=${CMAKE_CURRENT_BINARY_DIR}/vendor/secp256k1-zkp --with-bignum=no --enable-tests=no --enable-benchmark=no --host=x86_64-w64-mingw32
     BUILD_COMMAND make
     INSTALL_COMMAND true
     BUILD_BYPRODUCTS ${SEC256K1_BINARY_DIR}/.libs/libsecp256k1.a ${SEC256K1_BINARY_DIR}/.libs/libsecp256k1.so
    )
    else ( MINGW )
    ExternalProject_Add( project_secp256k1
     PREFIX ${CMAKE_CURRENT_BINARY_DIR}/vendor/secp256k1-zkp
     SOURCE_DIR ${SECP256K1_SRC_DIR}
     BINARY_DIR ${SEC256K1_BINARY_DIR}
     CONFIGURE_COMMAND ${SECP256K1_SRC_DIR}/configure CFLAGS=${FPIC_FLAG} --prefix=${CMAKE_CURRENT_BINARY_DIR}/vendor/secp256k1-zkp --with-bignum=no --enable-tests=no --enable-benchmark=no --enable-module-recovery --enable-experimental --enable-module-generator --enable-module-rangeproof
     BUILD_COMMAND make
     INSTALL_COMMAND true
     BUILD_BYPRODUCTS ${SEC256K1_BINARY_DIR}/.libs/libsecp256k1.a ${SEC256K1_BINARY_DIR}/.libs/libsecp256k1.so
    )
    endif ( MINGW )
    ExternalProject_Add_Step(project_secp256k1 autogen
     WORKING_DIRECTORY ${SECP256K1_SRC_DIR}
     COMMAND ${SECP256K1_SRC_DIR}/autogen.sh
     DEPENDERS configure
    )

    ExternalProject_Get_Property(project_secp256k1 binary_dir)

    add_library(secp256k1 STATIC IMPORTED)
    add_library(secp256k1_shared SHARED IMPORTED)
    set_property(TARGET secp256k1 PROPERTY IMPORTED_LOCATION ${binary_dir}/.libs/libsecp256k1${CMAKE_STATIC_LIBRARY_SUFFIX})
    set_property(TARGET secp256k1_shared PROPERTY IMPORTED_LOCATION ${binary_dir}/.libs/libsecp256k1${CMAKE_SHARED_LIBRARY_SUFFIX})
    set_property(TARGET secp256k1 PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/vendor/secp256k1-zkp/include)
    set_property(TARGET secp256k1_shared PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/vendor/secp256k1-zkp/include)
    add_dependencies(secp256k1 project_secp256k1)
    add_dependencies(secp256k1_shared project_secp256k1)
endif ( MSVC )
# End configure secp256k1-zkp

IF( WIN32 )
  MESSAGE(STATUS "Configuring fc to build on Win32")

  set( RPCRT4 rpcrt4 )

  #boost
  SET(BOOST_ROOT $ENV{BOOST_ROOT})
#  set(Boost_USE_DEBUG_PYTHON ON)
  set(Boost_USE_MULTITHREADED ON)
  set(BOOST_ALL_DYN_LINK OFF) # force dynamic linking for all libraries

  set( PLATFORM_SPECIFIC_LIBS wsock32.lib ws2_32.lib userenv.lib)
  # iphlpapi.lib

ELSE(WIN32)
  MESSAGE(STATUS "Configuring fc to build on Unix/Apple")

  IF(NOT APPLE)
    # Linux or other unix
    SET(rt_library rt )
    SET(pthread_library pthread)
  ENDIF(NOT APPLE)
ENDIF(WIN32)



IF(NOT "$ENV{OPENSSL_ROOT_DIR}" STREQUAL "")
  set(OPENSSL_ROOT_DIR $ENV{OPENSSL_ROOT_DIR} )
  set(OPENSSL_INCLUDE_DIR ${OPENSSL_ROOT_DIR}/include)
  message(STATUS "Setting up OpenSSL root and include vars to ${OPENSSL_ROOT_DIR}, ${OPENSSL_INCLUDE_DIR}")
ENDIF()

option( UNITY_BUILD OFF )

set( fc_sources
     src/uint128.cpp
     src/real128.cpp
     src/variant.cpp
     src/exception.cpp
     src/variant_object.cpp
     src/thread/thread.cpp
     src/thread/thread_specific.cpp
     src/thread/future.cpp
     src/thread/task.cpp
     src/thread/spin_lock.cpp
     src/thread/spin_yield_lock.cpp
     src/thread/mutex.cpp
     src/thread/non_preemptable_scope_check.cpp
     src/asio.cpp
     src/stacktrace.cpp
     src/string.cpp
     src/shared_ptr.cpp
     src/time.cpp
     src/utf8.cpp
     src/io/iostream.cpp
     src/io/datastream.cpp
     src/io/buffered_iostream.cpp
     src/io/fstream.cpp
     src/io/sstream.cpp
     src/io/json.cpp
     src/io/varint.cpp
     src/io/console.cpp
     src/filesystem.cpp
     src/interprocess/signals.cpp
     src/interprocess/file_mapping.cpp
     src/interprocess/mmap_struct.cpp
     src/interprocess/file_mutex.cpp
     src/rpc/cli.cpp
     src/rpc/http_base_api.cpp
     src/rpc/http_api.cpp
     src/rpc/json_connection.cpp
     src/rpc/state.cpp
     src/rpc/bstate.cpp
     src/rpc/websocket_api.cpp
     src/log/log_message.cpp
     src/log/logger.cpp
     src/log/appender.cpp
     src/log/console_appender.cpp
     src/log/file_appender.cpp
     src/log/gelf_appender.cpp
     src/log/logger_config.cpp
     src/crypto/_digest_common.cpp
     src/crypto/openssl.cpp
     src/crypto/aes.cpp
     src/crypto/crc.cpp
     src/crypto/city.cpp
     src/crypto/base32.cpp
     src/crypto/base36.cpp
     src/crypto/base58.cpp
     src/crypto/base64.cpp
     src/crypto/bigint.cpp
     src/crypto/hex.cpp
     src/crypto/sha1.cpp
     src/crypto/ripemd160.cpp
     src/crypto/cripemd160.cpp
     src/crypto/sha256.cpp
     src/crypto/sha224.cpp
     src/crypto/sha512.cpp
     src/crypto/blowfish.cpp
     src/crypto/elliptic_common.cpp
     src/crypto/equihash.cpp
     src/crypto/restartable_sha256.cpp
     ${ECC_REST}
     src/crypto/elliptic_${ECC_IMPL}.cpp
     src/crypto/rand.cpp
     src/network/tcp_socket.cpp
     src/network/tcp_ssl_socket.cpp
     src/network/udp_socket.cpp
     src/network/http/http_connection.cpp
     src/network/http/ssl_http_connection.cpp
     src/network/http/http_server.cpp
     src/network/http/websocket.cpp
     src/network/ntp.cpp
     src/network/ip.cpp
     src/network/rate_limiting.cpp
     src/network/resolve.cpp
     src/network/url.cpp
     src/network/gntp.cpp
     src/compress/smaz.cpp
     src/compress/zlib.cpp
     src/reflect/typename.cpp
     vendor/cyoencode-1.0.2/src/CyoDecode.c
     vendor/cyoencode-1.0.2/src/CyoEncode.c
     vendor/simdjson/simdjson.cpp
     )

file( GLOB_RECURSE fc_headers ${CMAKE_CURRENT_SOURCE_DIR} *.hpp *.h )

set( sources
  ${fc_sources}
)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/git_revision.cpp.in" "${CMAKE_CURRENT_BINARY_DIR}/git_revision.cpp" @ONLY)
list(APPEND sources "${CMAKE_CURRENT_BINARY_DIR}/git_revision.cpp")
list(APPEND sources ${fc_headers})

add_subdirectory( vendor/websocketpp )
add_subdirectory( vendor/equihash )

setup_library( fc_core SOURCES ${sources} LIBRARY_TYPE OBJECT DONT_INSTALL_LIBRARY )

# begin readline stuff
find_package(Curses)

set( Readline_USE_STATIC_LIBS ON CACHE BOOL "Use static readline libraries" )
find_package(Readline)

file(GLOB HEADERS "include/bts/cli/*.hpp")

if (READLINE_FOUND)
  target_compile_definitions (fc_core PRIVATE HAVE_READLINE)
  if (CURSES_FOUND)
    list(APPEND Readline_LIBRARIES ${CURSES_LIBRARY})
  endif()
  set(readline_includes ${Readline_INCLUDE_DIR})
endif()
if(WIN32)
  target_compile_definitions( fc_core PRIVATE _CRT_NONSTDC_NO_DEPRECATE )
endif(WIN32)
# end readline stuff

IF(WIN32)
  target_compile_definitions(fc_core PUBLIC WIN32 NOMINMAX _WIN32_WINNT=0x0501 _CRT_SECURE_NO_WARNINGS
    _SCL_SERCURE_NO_WARNINGS
    # Needed to disable MSVC autolinking feature (#pragma comment)
    BOOST_ALL_NO_LIB
    # The current version of websockets doesn't correctly guess what 'chrono' implementation boost::asio uses
    # on the recommended build platform of VC++12/boost_1.58.  Force it here until websocket gets their
    # autodetecting code to do the right thing.
    _WEBSOCKETPP_CPP11_CHRONO_
  )
  if( MSVC )
    # Activate C++ exception handling, assume extern C calls don't throw
    # Add /U options to be sure settings specific to dynamic boost link are ineffective
    target_compile_options(fc_core PUBLIC /EHsc /UBOOST_ALL_DYN_LINK /UBOOST_LINKING_PYTHON /UBOOST_DEBUG_PYTHON)
  elseif( MINGW )
     # Put MinGW specific compiler settings here
  endif()
ELSE()
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")

  IF(APPLE)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -Wall")
  ELSE()
    if( NOT "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" )
      target_compile_options(fc_core PUBLIC -Wall -fnon-call-exceptions)
    endif()
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fnon-call-exceptions")
  ENDIF()
ENDIF()

# This will become unnecessary once we update to websocketpp which fixes upstream issue #395
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DWEBSOCKETPP_STRICT_MASKING")

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DBOOST_ASIO_HAS_STD_CHRONO")

OPTION( LOG_LONG_API "Log long API calls over websocket (ON OR OFF)" ON )
MESSAGE( STATUS "LOG_LONG_API: ${LOG_LONG_API}" )
if( LOG_LONG_API )
  SET( LOG_LONG_API_MAX_MS 1000 CACHE STRING "Max API execution time in ms" )
  SET( LOG_LONG_API_WARN_MS 750 CACHE STRING "API execution time in ms at which to warn" )
  MESSAGE( STATUS "     " )
  MESSAGE( STATUS "     LOGGING LONG API CALLS" )
  MESSAGE( STATUS "          MAX  MS: ${LOG_LONG_API_MAX_MS}" )
  MESSAGE( STATUS "          WARN MS: ${LOG_LONG_API_WARN_MS}" )
  MESSAGE( STATUS "     " )
  SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DLOG_LONG_API -DLOG_LONG_API_MAX_MS=${LOG_LONG_API_MAX_MS} -DLOG_LONG_API_WARN_MS=${LOG_LONG_API_WARN_MS}" )
  SET( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DLOG_LONG_API -DLOG_LONG_API_MAX_MS=${LOG_LONG_API_MAX_MS} -DLOG_LONG_API_WARN_MS=${LOG_LONG_API_WARN_MS}" )
endif()

SET( public_includes
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${Boost_INCLUDE_DIR}
        ${OPENSSL_INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/websocketpp
        "${readline_includes}"
)

SET( private_includes
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/cyoencode-1.0.2/src
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/simdjson
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/secp256k1-zkp
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/equihash
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/bitcoin/src
)

target_include_directories(fc_core PUBLIC ${public_includes} PRIVATE ${private_includes})

target_link_libraries( fc_core PUBLIC ${LINK_USR_LOCAL_LIB} equihash ${PLATFORM_SPECIFIC_LIBS} ${RPCRT4} ${CMAKE_DL_LIBS} ${rt_library} ${Readline_LIBRARIES} ${ECC_LIB} )

if(MSVC)
  set_source_files_properties( src/network/http/websocket.cpp PROPERTIES COMPILE_FLAGS "/bigobj" )
endif(MSVC)


IF(NOT Boost_UNIT_TEST_FRAMEWORK_LIBRARY MATCHES "\\.(a|lib)$")
IF(MSVC)
add_definitions(/DBOOST_TEST_DYN_LINK)
ELSE(MSVC)
# add_definitions(-DBOOST_TEST_DYN_LINK)
ENDIF(MSVC)
ENDIF()

include_directories( vendor/websocketpp )
include_directories( vendor/equihash )

add_subdirectory(tests)

if(WIN32)
   # add addtional import library on windows platform
   target_link_libraries( fc_core PUBLIC crypt32.lib)

   # now generate a list of the DLLs we're using to use during the install process
   include (ParseLibraryList)
   PARSE_LIBRARY_LIST(${Boost_LIBRARIES}
                        FOUND parseOk
                        DEBUG Boost_LIBRARIES_DEBUG
                        OPT   Boost_LIBRARIES_RELEASE
                        GENERAL Boost_LIBRARIES_GENERAL)

   #Variable will hold list of .pdb files generated for libraries the 'fc' module is linked to
   set(INTERFACE_LINK_PDB_RELEASE)

   set(SHARED_LIBRARIES_RELEASE)
   foreach(boost_import_lib ${Boost_LIBRARIES_RELEASE})
      get_filename_component(import_lib_name_root ${boost_import_lib} NAME_WE)
      get_filename_component(import_lib_path ${boost_import_lib} PATH)
      set(boost_dll "${import_lib_path}/${import_lib_name_root}${CMAKE_SHARED_LIBRARY_SUFFIX}")
      set(boost_lib_pdb "${import_lib_name_root}.pdb")

      FILE(GLOB_RECURSE boost_pdb_paths "${import_lib_path}/../../bin.v2/*/${boost_lib_pdb}")

      foreach(p ${boost_pdb_paths})
        if(p MATCHES ".*/address-model-${platformBitness}/")
          GP_APPEND_UNIQUE(INTERFACE_LINK_PDB_RELEASE ${p})
        endif()
      endforeach()

      if(EXISTS "${boost_dll}")
        set(SHARED_LIBRARIES_RELEASE ${SHARED_LIBRARIES_RELEASE} "${boost_dll}")
      endif()

   endforeach()

   set(INTERFACE_LINK_PDB_DEBUG)
   set(SHARED_LIBRARIES_DEBUG)
   foreach(boost_import_lib ${Boost_LIBRARIES_DEBUG})
      get_filename_component(import_lib_name_root ${boost_import_lib} NAME_WE)
      get_filename_component(import_lib_path ${boost_import_lib} PATH)
      set(boost_dll "${import_lib_path}/${import_lib_name_root}${CMAKE_SHARED_LIBRARY_SUFFIX}")
      set(boost_lib_pdb "${import_lib_name_root}.pdb")

      FILE(GLOB_RECURSE boost_pdb_paths "${import_lib_path}/../../bin.v2/*/${boost_lib_pdb}")

      foreach(p ${boost_pdb_paths})
        if(p MATCHES ".*/address-model-${platformBitness}/")
          GP_APPEND_UNIQUE(INTERFACE_LINK_PDB_DEBUG ${p})
        endif()
      endforeach()
      if(EXISTS "${boost_dll}")
        set(SHARED_LIBRARIES_DEBUG ${SHARED_LIBRARIES_DEBUG} "${boost_dll}")
      endif()
   endforeach()

   # message(STATUS "openssl_libraries=${OPENSSL_LIBRARIES}")
   foreach(lib ${OPENSSL_LIBRARIES})
      get_filename_component(lib_name ${lib} NAME_WE)
      if (${lib_name} STREQUAL "libeay32")
         get_filename_component(lib_dir ${lib} DIRECTORY)
         get_filename_component(openssl_dir "${lib_dir}/.." REALPATH)
         set( eaydll "${openssl_dir}/bin/${lib_name}${CMAKE_SHARED_LIBRARY_SUFFIX}")
         set(eay_pdb "${openssl_dir}/bin/${lib_name}.pdb")
         message(STATUS "eay=${eaydll}")
         if(EXISTS ${eay_pdb})
           GP_APPEND_UNIQUE(INTERFACE_LINK_PDB_RELEASE ${eay_pdb})
           GP_APPEND_UNIQUE(INTERFACE_LINK_PDB_DEBUG ${eay_pdb})
         endif()

         set(SHARED_LIBRARIES_DEBUG   ${SHARED_LIBRARIES_DEBUG}   "${eaydll}")
         set(SHARED_LIBRARIES_RELEASE ${SHARED_LIBRARIES_RELEASE} "${eaydll}")
      endif()
   endforeach()

   set_property(TARGET fc_core PROPERTY INTERFACE_LINK_PDB_RELEASE ${INTERFACE_LINK_PDB_RELEASE})
   set_property(TARGET fc_core PROPERTY INTERFACE_LINK_PDB_DEBUG ${INTERFACE_LINK_PDB_DEBUG})
   set_property(TARGET fc_core PROPERTY SHARED_LIBRARIES_DEBUG ${SHARED_LIBRARIES_DEBUG})
   set_property(TARGET fc_core PROPERTY SHARED_LIBRARIES_RELEASE ${SHARED_LIBRARIES_RELEASE})

endif(WIN32)

ADD_LIBRARY( CommonBuildOptions INTERFACE)
TARGET_COMPILE_OPTIONS( CommonBuildOptions INTERFACE -Werror=return-local-addr )
TARGET_COMPILE_OPTIONS( CommonBuildOptions INTERFACE $<$<CONFIG:Asan>:${HIVE_ASAN_COMPILE_OPTIONS}>)
TARGET_LINK_OPTIONS( CommonBuildOptions INTERFACE $<$<CONFIG:Asan>:${HIVE_ASAN_LINK_OPTIONS}>)

TARGET_LINK_LIBRARIES( fc_core PUBLIC CommonBuildOptions )

ADD_LIBRARY( SHARED_BOOST INTERFACE )
SET_TARGET_PROPERTIES( SHARED_BOOST PROPERTIES POSITION_INDEPENDENT_CODE ON )
ADD_TARGET_BOOST_LIBRARIES( SHARED_BOOST )

ADD_LIBRARY( STATIC_BOOST INTERFACE )
SET_TARGET_PROPERTIES( STATIC_BOOST PROPERTIES POSITION_INDEPENDENT_CODE OFF )
ADD_TARGET_BOOST_LIBRARIES( STATIC_BOOST )

ADD_LIBRARY ( SHARED_OPENSSL INTERFACE )
ADD_TARGET_OPENSSL_LIBRARIES (SHARED_OPENSSL FALSE)

ADD_LIBRARY ( STATIC_OPENSSL INTERFACE )
ADD_TARGET_OPENSSL_LIBRARIES (STATIC_OPENSSL TRUE)

add_library ( fc STATIC )
target_link_libraries ( fc PUBLIC fc_core STATIC_BOOST STATIC_OPENSSL )
ADD_TARGET_PACKAGE_LIBRARIES( fc ZLIB TRUE )
ADD_TARGET_PACKAGE_LIBRARIES( fc BZip2 TRUE )

add_library ( fc_shared_boost STATIC )
target_link_libraries ( fc_shared_boost PUBLIC fc_core SHARED_BOOST SHARED_OPENSSL )
ADD_TARGET_PACKAGE_LIBRARIES( fc_shared_boost ZLIB FALSE )
ADD_TARGET_PACKAGE_LIBRARIES( fc_shared_boost BZip2 FALSE )

SET(OPENSSL_CONF_TARGET )
IF(DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
  SET (OPENSSL_CONF_TARGET ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
ELSE()
  SET (OPENSSL_CONF_TARGET ${CMAKE_CURRENT_BINARY_DIR})
ENDIF()

    IF(WIN32)
       IF("${OPENSSL_ROOT_DIR}" STREQUAL "")
          get_filename_component(OPENSSL_ROOT_DIR "${OPENSSL_INCLUDE_DIR}/.." REALPATH)
       ENDIF()
       SET(OPENSSL_CONF_SOURCE "${OPENSSL_ROOT_DIR}/ssl/openssl.cnf")
       IF(MINGW)
          SET(OPENSSL_CONF_SOURCE "${OPENSSL_ROOT_DIR}/openssl.cnf")
       ENDIF(MINGW)
       SET(POST_BUILD_STEP_COMMANDS ${POST_BUILD_STEP_COMMANDS}
           COMMAND ${CMAKE_COMMAND} -E copy_if_different "${OPENSSL_CONF_SOURCE}" "${OPENSSL_CONF_TARGET}/openssl.cnf")
    ENDIF(WIN32)

ADD_CUSTOM_COMMAND(TARGET fc POST_BUILD ${POST_BUILD_STEP_COMMANDS}
  COMMENT "Copying OpenSSL/ssl/openssl.cnf into target directory."
)

ADD_CUSTOM_COMMAND(TARGET fc_shared_boost POST_BUILD ${POST_BUILD_STEP_COMMANDS}
  COMMENT "Copying OpenSSL/ssl/openssl.cnf into target directory."
)

MESSAGE(STATUS "Finished fc module configuration...")
