# This file encapsulates custom build configuration types to be specified to CMake. It must be included before first `project()` or `enable_language()` statement.

cmake_policy(SET CMP0057 NEW) # Related to IN_LIST operator behavior

GET_PROPERTY(isMultiConfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)

if(isMultiConfig)
    if(NOT "Asan" IN_LIST CMAKE_CONFIGURATION_TYPES)
        LIST(APPEND CMAKE_CONFIGURATION_TYPES Asan)
    endif()
else()
    SET(allowedBuildTypes )
    LIST(APPEND allowedBuildTypes Asan Debug Release RelWithDebInfo)

    IF(NOT CMAKE_BUILD_TYPE)
      SET(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Choose the type of build, options are: ${allowedBuildTypes}" FORCE)
      MESSAGE(WARNING "Implicit build type chosen: ${CMAKE_BUILD_TYPE}")
    ELSEIF(NOT (CMAKE_BUILD_TYPE IN_LIST allowedBuildTypes))
      MESSAGE(FATAL_ERROR "Specified build type: ${CMAKE_BUILD_TYPE} does not match any predefined configuration: ${allowedBuildTypes}")
    ENDIF()

    SET_PROPERTY(CACHE CMAKE_BUILD_TYPE PROPERTY PROPERTY HELPSTRING "Choose the type of build" STRINGS "${allowedBuildTypes}")
endif()

# set base configuration to initialize ASAN settings
STRING(TOUPPER "RelWithDebInfo" asanBaseConfig)

SET(HIVE_ASAN_COMPILE_OPTIONS -fsanitize=address -fno-omit-frame-pointer -fno-ipa-icf -fno-optimize-sibling-calls)
SET(HIVE_ASAN_LINK_OPTIONS -static-libasan -fsanitize=address)

set(CMAKE_C_FLAGS_ASAN
    "${CMAKE_C_FLAGS_${asanBaseConfig}} " CACHE STRING
    "Flags used by the C compiler for Asan build type or configuration." FORCE)

MESSAGE(STATUS "CMAKE_C_FLAGS_ASAN value is based on: CMAKE_C_FLAGS_${asanBaseConfig} and points to value: ${CMAKE_C_FLAGS_ASAN}" )

set(CMAKE_CXX_FLAGS_ASAN
    "${CMAKE_CXX_FLAGS_${asanBaseConfig}} " CACHE STRING
    "Flags used by the C++ compiler for Asan build type or configuration." FORCE)

MESSAGE(STATUS "CMAKE_CXX_FLAGS_ASAN value is based on: CMAKE_CXX_FLAGS_${asanBaseConfig} and points to value: ${CMAKE_CXX_FLAGS_ASAN}" )

set(CMAKE_EXE_LINKER_FLAGS_ASAN
    "${CMAKE_EXE_LINKER_FLAGS_${asanBaseConfig}} " CACHE STRING
    "Linker flags to be used to create executables for Asan build type." FORCE)

MESSAGE(STATUS "CMAKE_EXE_LINKER_FLAGS_ASAN value is based on: CMAKE_EXE_LINKER_FLAGS_${asanBaseConfig} and points to value: ${CMAKE_EXE_LINKER_FLAGS_ASAN}" )

set(CMAKE_SHARED_LINKER_FLAGS_ASAN
    "${CMAKE_SHARED_LINKER_FLAGS_${asanBaseConfig}} " CACHE STRING
    "Linker lags to be used to create shared libraries for Asan build type." FORCE)

MESSAGE(STATUS "CMAKE_SHARED_LINKER_FLAGS_ASAN value is based on: CMAKE_SHARED_LINKER_FLAGS_${asanBaseConfig} and points to value: ${CMAKE_SHARED_LINKER_FLAGS_ASAN}" )
