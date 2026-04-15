# for each subdirectory containing a CMakeLists.txt, add that subdirectory

set(BUILD_SHARED_LIBS OFF)
set(ROCKSDB_STATIC ON)

#Configure needed build options of RocksDB.
set(ROCKSDB_BUILD_SHARED OFF CACHE BOOL "Build shared versions of the RocksDB libraries")
SET(FAIL_ON_WARNINGS OFF CACHE BOOL "Treat compile warnings as errors")
SET(WITH_TESTS OFF CACHE BOOL "build with tests")
SET(WITH_SNAPPY ON CACHE BOOL "build with SNAPPY")
SET(WITH_ZLIB OFF CACHE BOOL "build with ZLIB")
SET(WITH_BZ2 OFF CACHE BOOL "build with BZ2")
SET(WITH_BENCHMARK_TOOLS OFF CACHE BOOL "build with BENCHMARKS")
SET(WITH_CORE_TOOLS ${BUILD_HIVE_TESTNET} CACHE BOOL "build with core tools")
SET(WITH_TOOLS OFF CACHE BOOL "build with tools")
SET(WITH_TRACE_TOOLS OFF CACHE BOOL "build with trace tools")
SET(WITH_GFLAGS OFF CACHE BOOL "build with GFlags")
SET(USE_RTTI ON CACHE BOOL "build with RTTI")
SET(PORTABLE ON CACHE BOOL "build a portable binary")
SET(WITH_LIBURING ON CACHE BOOL "build with io_uring support")
SET(WITH_TESTS OFF CACHE BOOL "build with tests")
SET(WITH_TOOLS OFF CACHE BOOL "build with tools")

# Enable RocksDB sanitizers when building with Asan
if(CMAKE_BUILD_TYPE STREQUAL "Asan")
  SET(WITH_ASAN ON CACHE BOOL "build with ASAN" FORCE)
endif()

# If we don't have CMake variables defined, try to get them from the environment
if (NOT DEFINED ZLIB_ROOT_DIR)
   if (EXISTS "$ENV{ZLIB_ROOT_DIR}")
      set(ZLIB_ROOT_DIR $ENV{ZLIB_ROOT_DIR})
   endif()
endif()
if (NOT DEFINED BZIP2_ROOT_DIR)
   if (EXISTS "$ENV{BZIP2_ROOT_DIR}")
      set(BZIP2_ROOT_DIR $ENV{BZIP2_ROOT_DIR})
   endif()
endif()

find_package( Snappy CONFIG )

# file( GLOB children RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} * )
# foreach( child ${children} )
#    if( IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${child}" )
#       if( EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${child}/CMakeLists.txt" )
#          add_subdirectory( "${child}" )
#       endif()
#    endif()
# endforeach()

# Prevent RocksDB from setting GLOBAL RULE_LAUNCH_COMPILE to ccache.
# Compiler caching is handled by the top-level CMakeLists.txt via
# CMAKE_C_COMPILER_LAUNCHER / CMAKE_CXX_COMPILER_LAUNCHER.
# RocksDB's approach conflicts with that (double-wrapping the compiler).
set(CCACHE_FOUND "CCACHE_FOUND-NOTFOUND" CACHE FILEPATH "Disabled: compiler caching handled at top level" FORCE)

# --- Preinstalled RocksDB detection ---
set(HIVE_VENDOR_PREINSTALLED_DIR "/opt/hive/vendor" CACHE PATH
    "Base directory for preinstalled vendor libraries")
option(HIVE_USE_PREINSTALLED_VENDOR
    "Use preinstalled vendor libraries if available (OFF to always build from source)" ON)

# Verify submodule is initialized
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/rocksdb/.git")
  message(FATAL_ERROR
    "RocksDB submodule not initialized. Run: git submodule update --init --recursive")
endif()

# Read the submodule commit hash
execute_process(
  COMMAND git rev-parse HEAD
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/rocksdb"
  OUTPUT_VARIABLE _rocksdb_commit
  OUTPUT_STRIP_TRAILING_WHITESPACE
  RESULT_VARIABLE _rocksdb_git_result
)

set(_rocksdb_preinstalled_dir
    "${HIVE_VENDOR_PREINSTALLED_DIR}/rocksdb/${_rocksdb_commit}")

set(_use_preinstalled OFF)
if(HIVE_USE_PREINSTALLED_VENDOR
   AND _rocksdb_git_result EQUAL 0
   AND EXISTS "${_rocksdb_preinstalled_dir}/lib/librocksdb.a"
   AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug"
   AND NOT CMAKE_BUILD_TYPE STREQUAL "Asan")

  # Ensure uring::uring is available before find_package(RocksDB).
  # RocksDBConfig.cmake does NOT call find_dependency(uring), but
  # RocksDBTargets.cmake references uring::uring in INTERFACE_LINK_LIBRARIES.
  # The main CMakeLists.txt includes Finduring.cmake (line 158) before
  # add_subdirectory(libraries), so uring::uring should already exist.
  # As a safety net, add the RocksDB cmake/modules to MODULE_PATH so that
  # Finduring.cmake can be found if needed by any transitive resolution.
  list(APPEND CMAKE_MODULE_PATH
       "${CMAKE_CURRENT_SOURCE_DIR}/rocksdb/cmake/modules"
       "${_rocksdb_preinstalled_dir}/lib/cmake/rocksdb/modules")

  # Use find_package with NO_DEFAULT_PATH to prevent finding a system-installed
  # RocksDB instead of our commit-matched preinstalled version.
  find_package(RocksDB CONFIG QUIET
    PATHS "${_rocksdb_preinstalled_dir}"
    NO_DEFAULT_PATH)

  if(RocksDB_FOUND)
    message(STATUS "Using preinstalled RocksDB (commit: ${_rocksdb_commit})")
    message(STATUS "  Path: ${_rocksdb_preinstalled_dir}")

    # Diagnostic: check what the exported target provides
    get_target_property(_rdb_inc RocksDB::rocksdb INTERFACE_INCLUDE_DIRECTORIES)
    message(STATUS "  RocksDB::rocksdb INTERFACE_INCLUDE_DIRECTORIES: ${_rdb_inc}")

    # Create an INTERFACE library wrapping the IMPORTED target.
    # Using INTERFACE instead of ALIAS ensures include directories are always
    # propagated correctly, regardless of how the exported target was configured.
    # The rest of the hive codebase links against the bare name "rocksdb".
    add_library(rocksdb INTERFACE)
    target_link_libraries(rocksdb INTERFACE RocksDB::rocksdb)
    target_include_directories(rocksdb SYSTEM INTERFACE
      "${_rocksdb_preinstalled_dir}/include")

    set(_use_preinstalled ON)

    # Expose sst_dump path for build.sh.
    # Use PROJECT_BINARY_DIR (not CMAKE_BINARY_DIR) so the file lands in the
    # hive subdirectory of the build tree when built from HAF via add_subdirectory(hive).
    file(WRITE "${PROJECT_BINARY_DIR}/rocksdb_tools_dir.txt"
         "${_rocksdb_preinstalled_dir}/bin")
  else()
    message(STATUS "Preinstalled RocksDB found but find_package failed, building from source")
  endif()
endif()

if(NOT _use_preinstalled)
  if(_rocksdb_git_result EQUAL 0 AND HIVE_USE_PREINSTALLED_VENDOR)
    message(STATUS "No preinstalled RocksDB for commit ${_rocksdb_commit} "
                   "(or Debug/Asan build), building from source")
  elseif(NOT HIVE_USE_PREINSTALLED_VENDOR)
    message(STATUS "Preinstalled vendor libraries disabled, building RocksDB from source")
  else()
    message(STATUS "Cannot read RocksDB submodule commit, building from source")
  endif()

  # Current behavior — build from source
  add_subdirectory(rocksdb)
endif()

GET_TARGET_PROPERTY( prop Snappy::snappy IMPORTED_LOCATION_RELWITHDEBINFO )

MESSAGE ( STATUS "Snappy::snappy (IMPORTED_LOCATION): ${prop}" )

# Seems that nowadays versions of smappy library define just single target:

IF ( TARGET Snappy::snappy-static )
  GET_TARGET_PROPERTY( prop Snappy::snappy-static IMPORTED_LOCATION_RELWITHDEBINFO )
  MESSAGE ( STATUS "Snappy::snappy-static (IMPORTED_LOCATION): ${prop}, overriding standard target path to force static linking" )
  SET_TARGET_PROPERTIES( Snappy::snappy PROPERTIES IMPORTED_LOCATION ${prop} IMPORTED_LOCATION_RELWITHDEBINFO ${prop} )
ENDIF()

SET(ZSTD_BUILD_SHARED OFF)
SET(ZSTD_BUILD_STATIC ON)
SET(ZSTD_BUILD_CONTRIB OFF)
SET(ZSTD_BUILD_TESTS OFF)
SET(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "Build ZSTD programs")
add_subdirectory(zstd/build/cmake)
