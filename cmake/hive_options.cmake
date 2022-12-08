
OPTION( BUILD_HIVE_TESTNET "Build source for test network (ON OR OFF)" OFF )
MESSAGE( STATUS "BUILD_HIVE_TESTNET: ${BUILD_HIVE_TESTNET}" )
if( BUILD_HIVE_TESTNET  )
  MESSAGE( STATUS "     " )
  MESSAGE( STATUS "             CONFIGURING FOR TEST NET             " )
  MESSAGE( STATUS "     " )
  SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DIS_TEST_NET" )
  SET( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DIS_TEST_NET" )
endif()

OPTION( ENABLE_SMT_SUPPORT "Build source with SMT support (ON OR OFF)" OFF )
MESSAGE( STATUS "ENABLE_SMT_SUPPORT: ${ENABLE_SMT_SUPPORT}" )
if( ENABLE_SMT_SUPPORT  )
  MESSAGE( STATUS "     " )
  MESSAGE( STATUS "             CONFIGURING FOR SMT SUPPORT             " )
  MESSAGE( STATUS "     " )
  SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHIVE_ENABLE_SMT" )
  SET( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DHIVE_ENABLE_SMT" )
endif()

OPTION( STORE_ACCOUNT_METADATA "Keep the json_metadata for accounts (ON OR OFF)" ON )
MESSAGE( STATUS "STORE_ACCOUNT_METADATA: ${STORE_ACCOUNT_METADATA}" )
if( STORE_ACCOUNT_METADATA )
  MESSAGE( STATUS "     " )
  MESSAGE( STATUS "                 CONFIGURING TO INDEX ACCOUNT METADATA      " )
  MESSAGE( STATUS "     " )
  SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCOLLECT_ACCOUNT_METADATA" )
  SET( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DCOLLECT_ACCOUNT_METADATA" )
endif()

OPTION( CHAINBASE_CHECK_LOCKING "Check locks in chainbase (ON or OFF)" ON )
MESSAGE( STATUS "CHAINBASE_CHECK_LOCKING: ${CHAINBASE_CHECK_LOCKING}" )
if( CHAINBASE_CHECK_LOCKING )
  SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCHAINBASE_CHECK_LOCKING" )
  SET( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DCHAINBASE_CHECK_LOCKING" )
endif()

OPTION( HIVE_CONVERTER_BUILD "Build hive blockchain converter (ON or OFF)" OFF )
if( HIVE_CONVERTER_BUILD )
  SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHIVE_CONVERTER_BUILD" )
  SET( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DHIVE_CONVERTER_BUILD" )
endif()
MESSAGE( STATUS "HIVE_CONVERTER_BUILD: ${HIVE_CONVERTER_BUILD}" )

if( HIVE_CONVERTER_BUILD OR BUILD_HIVE_TESTNET )
  MESSAGE( STATUS "CONFIGURING FOR ALTERNATE CHAIN ID USAGE" )
  SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_ALTERNATE_CHAIN_ID" )
  SET( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DUSE_ALTERNATE_CHAIN_ID" )
endif()

OPTION( HIVE_LINT "Enable linting with clang-tidy during compilation" OFF )

if( HIVE_LINT )
  find_program(
    CLANG_TIDY_EXE
    NAMES "clang-tidy"
    DOC "Path to clain-tidy executable"
  )

  SET( CLANG_TIDY_IGNORED
  "-fuchsia-default-arguments-*\
  ,-clang-diagnostic-unknown-warning-option\
  ,-bugprone-lambda-function-name\
  ,-llvm-qualified-auto\
  ,-misc-non-private-member-variables-in-classes\
  ,-cert-dcl16-c\
  ,-bugprone-narrowing-conversions\
  ,-bugprone-macro-parentheses\
  ,-bugprone-branch-clone\
  ,-cert-oop54-cpp\
  ,-fuchsia-statically-constructed-objects\
  ,-performance-unnecessary-value-param\
  ,-bugprone-exception-escape\
  ,-clang-analyzer-cplusplus.NewDeleteLeaks\
  ,-fuchsia-default-arguments\
  ,-fuchsia-trailing-return\
  ,-clang-analyzer-optin.cplusplus.UninitializedObject\
  ,-bugprone-signed-char-misuse\
  ,-hicpp-*\
  ,-cert-err60-cpp\
  ,-llvm-namespace-comment\
  ,-cert-err09-cpp\
  ,-cert-err61-cpp\
  ,-fuchsia-overloaded-operator\
  ,-misc-throw-by-value-catch-by-reference\
  ,-misc-unused-parameters\
  ,-clang-analyzer-core.uninitialized.Assign\
  ,-llvm-include-order\
  ,-clang-diagnostic-unused-lambda-capture\
  ,-misc-macro-parentheses\
  ,-boost-use-to-string\
  ,-misc-lambda-function-name\
  ,-cert-err58-cpp\
  ,-cert-err34-c\
  ,-cppcoreguidelines-*\
  ,-modernize-*\
  ,-clang-diagnostic-#pragma-messages\
  ,-google-*\
  ,-readability-*"
  )

  if( NOT CLANG_TIDY_EXE )
    message( FATAL_ERROR "Clang tidy tool is not found, but is required by enabled HIVE_LINT option" )
  elseif( VERSION LESS 3.6 )
    message( FATAL_ERROR "clang-tidy found but only supported with CMake version >= 3.6" )
  else()
    message( STATUS "clany-tidy found: ${CLANG_TIDY_EXE}" )
    EXECUTE_PROCESS(
      COMMAND ${CLANG_TIDY_EXE} --version
      OUTPUT_VARIABLE CLANG_VERSION
      ERROR_QUIET
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    STRING( REPLACE "\n" ";" CLANG_VERSION ${CLANG_VERSION} )
    LIST( GET CLANG_VERSION 1 CLANG_VERSION )
    STRING( REGEX MATCH "[0-9]+" CLANG_VERSION ${CLANG_VERSION} )
    MESSAGE( STATUS "clang-tidy version: ${CLANG_VERSION}" )
    SET( CLANG_ALL_CHECKS "--checks=*" )
    IF ( CLANG_VERSION LESS 10 )
      SET( CLANG_ALL_CHECKS "-checks=*" )
    ENDIF()

    set( DO_CLANG_TIDY ${CLANG_TIDY_EXE};${CLANG_ALL_CHECKS},${CLANG_TIDY_IGNORED};--warnings-as-errors=* )
  endif( NOT CLANG_TIDY_EXE )
endif( HIVE_LINT )

OPTION( HIVE_FAIL_ON_WARNINGS "Treat compile warnings as errors. Auto-enabled with HIVE_LINT" HIVE_LINT )
if( HIVE_FAIL_ON_WARNINGS )
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror" )
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror" )
endif()
MESSAGE( STATUS "HIVE_FAIL_ON_WARNINGS: ${HIVE_FAIL_ON_WARNINGS}" )

set(ENABLE_COVERAGE_TESTING FALSE CACHE BOOL "Build Hive for code coverage analysis")

if(ENABLE_COVERAGE_TESTING)
    SET(CMAKE_CXX_FLAGS "--coverage ${CMAKE_CXX_FLAGS}")
endif()

# fc/src/compress/miniz.c breaks strict aliasing. The Linux kernel builds with no strict aliasing
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-strict-aliasing -DBOOST_THREAD_DONT_PROVIDE_PROMISE_LAZY" )
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-strict-aliasing -DBOOST_THREAD_DONT_PROVIDE_PROMISE_LAZY" )
