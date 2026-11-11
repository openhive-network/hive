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

# rocksdb_db_bench_tool: RocksDB's db_bench (db_bench_tool() without its main), which
# programs/util's hive_db_bench wraps with hive's comparators and column families. RocksDB builds
# db_bench only together with gflags in librocksdb itself (WITH_GFLAGS), so the sources are
# compiled here instead, with the flags, definitions and include directories of RocksDB's own
# directory and gflags linked into this library alone: hived never links gflags. A function, so
# the variables set for gflags' build stay local to it.
function(hive_add_db_bench_tool)
  find_package(gflags CONFIG QUIET)
  if(NOT TARGET gflags::gflags)
    include(FetchContent)
    FetchContent_Declare(gflags
      URL https://github.com/gflags/gflags/archive/refs/tags/v2.2.2.tar.gz
      URL_HASH SHA256=34af2f15cf7367513b352bdcd2493ab14ce43692d2dcd9dfc499492966c64dcf)
    FetchContent_GetProperties(gflags)
    if(NOT gflags_POPULATED)
      FetchContent_Populate(gflags)
    endif()
    set(GFLAGS_IS_SUBPROJECT TRUE)
    set(BUILD_TESTING OFF)
    set(BUILD_SHARED_LIBS OFF)
    set(BUILD_STATIC_LIBS ON)
    set(BUILD_gflags_LIB ON)
    set(BUILD_gflags_nothreads_LIB OFF)
    add_subdirectory("${gflags_SOURCE_DIR}" "${gflags_BINARY_DIR}" EXCLUDE_FROM_ALL)
  endif()

  set(_rocksdb_dir "${CMAKE_CURRENT_SOURCE_DIR}/rocksdb")
  # db_bench links the compression managers of these files, which subclass the classes whose
  # typeinfo compression.cc no longer emits (see the RTTI note below), as sst_dump does with
  # db_stress_compression_manager.cc; hived does not link them.
  if(NOT MSVC)
    set_source_files_properties("${_rocksdb_dir}/util/auto_tune_compressor.cc"
      "${_rocksdb_dir}/util/simple_mixed_compressor.cc"
      DIRECTORY "${_rocksdb_dir}"
      PROPERTIES COMPILE_OPTIONS "-fno-rtti;-UROCKSDB_USE_RTTI")
  endif()
  add_library(rocksdb_db_bench_tool STATIC EXCLUDE_FROM_ALL
    "${_rocksdb_dir}/tools/db_bench_tool.cc"
    "${_rocksdb_dir}/tools/simulated_hybrid_file_system.cc"
    "${_rocksdb_dir}/tools/tool_hooks.cc")
  get_directory_property(_definitions DIRECTORY "${_rocksdb_dir}" COMPILE_DEFINITIONS)
  get_directory_property(_includes DIRECTORY "${_rocksdb_dir}" INCLUDE_DIRECTORIES)
  get_directory_property(_cxx_flags DIRECTORY "${_rocksdb_dir}" DEFINITION CMAKE_CXX_FLAGS)
  separate_arguments(_cxx_flags UNIX_COMMAND "${_cxx_flags}")
  target_compile_definitions(rocksdb_db_bench_tool PRIVATE ${_definitions} GFLAGS=1)
  target_include_directories(rocksdb_db_bench_tool PRIVATE ${_includes} "${_rocksdb_dir}")
  target_compile_options(rocksdb_db_bench_tool PRIVATE ${_cxx_flags})
  target_link_libraries(rocksdb_db_bench_tool PUBLIC rocksdb gflags::gflags)
endfunction()

# --- Preinstalled RocksDB detection ---
set(HIVE_VENDOR_PREINSTALLED_DIR "/opt/hive/vendor" CACHE PATH
    "Base directory for preinstalled vendor libraries")
option(HIVE_USE_PREINSTALLED_VENDOR
    "Use preinstalled vendor libraries if available (OFF to always build from source)" ON)
option(HIVE_BUILD_DB_BENCH
    "Add the hive_db_bench target (RocksDB's db_bench for replaying hived's RocksDB traces, see doc/devs/rocksdb-trace-replay.md). Builds RocksDB from source and fetches gflags when it is not installed" OFF)

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
   AND NOT HIVE_BUILD_DB_BENCH)

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
  else()
    message(STATUS "Preinstalled RocksDB found but find_package failed, building from source")
  endif()
endif()

if(NOT _use_preinstalled)
  if(HIVE_BUILD_DB_BENCH)
    message(STATUS "hive_db_bench compiles RocksDB's own sources, building RocksDB from source")
  elseif(_rocksdb_git_result EQUAL 0 AND HIVE_USE_PREINSTALLED_VENDOR)
    message(STATUS "No preinstalled RocksDB for commit ${_rocksdb_commit} "
                   "(or Debug build), building from source")
  elseif(NOT HIVE_USE_PREINSTALLED_VENDOR)
    message(STATUS "Preinstalled vendor libraries disabled, building RocksDB from source")
  else()
    message(STATUS "Cannot read RocksDB submodule commit, building from source")
  endif()

  # Current behavior - build from source
  # The pinned RocksDB predates the <cstdint> include hygiene that newer
  # libstdc++ headers require (uint64_t etc. no longer provided transitively;
  # hard error with GCC 15 on Ubuntu 26.04). Force-include it for this subtree.
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 13)
    add_compile_options("SHELL:$<$<COMPILE_LANGUAGE:CXX>:-include cstdint>")
  endif()
  add_subdirectory(rocksdb)
  # RocksDB 11 compresses Snappy blocks through a local subclass of snappy::Sink. Compiled with RTTI
  # (USE_RTTI above), that object needs snappy's typeinfo, which upstream snappy builds (-fno-rtti
  # since 1.1.9, e.g. the static library of the CI base image) do not provide, so linking fails.
  # Nothing casts or typeid()s the classes of that file, so it is compiled without RTTI. That leaves
  # the typeinfo of the classes whose key functions it defines (Decompressor, CompressionManager)
  # unemitted, so their subclass in db_stress_compression_manager.cc, which sst_dump (hive's,
  # programs/util) and ldb link, is compiled without RTTI too.
  if(NOT MSVC)
    set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/rocksdb/util/compression.cc"
      "${CMAKE_CURRENT_SOURCE_DIR}/rocksdb/db_stress_tool/db_stress_compression_manager.cc"
      DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/rocksdb"
      PROPERTIES COMPILE_OPTIONS "-fno-rtti;-UROCKSDB_USE_RTTI")
  endif()
  if(HIVE_BUILD_DB_BENCH)
    hive_add_db_bench_tool()
  endif()
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
