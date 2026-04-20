# This file encapsulates custom build configuration types to be specified to CMake. It must be included before first `project()` or `enable_language()` statement.
#
# Sanitizer build types (Asan, Msan) are mutually exclusive — CMAKE_BUILD_TYPE
# accepts only a single value, so only one sanitizer can be active per build.
# Each sanitizer is defined via define_sanitizer_build_type() below.

cmake_policy(SET CMP0057 NEW) # Related to IN_LIST operator behavior

GET_PROPERTY(isMultiConfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)

if(isMultiConfig)
    if(NOT "Asan" IN_LIST CMAKE_CONFIGURATION_TYPES)
        LIST(APPEND CMAKE_CONFIGURATION_TYPES Asan)
    endif()
    if(NOT "Msan" IN_LIST CMAKE_CONFIGURATION_TYPES)
        LIST(APPEND CMAKE_CONFIGURATION_TYPES Msan)
    endif()
else()
    SET(allowedBuildTypes )
    LIST(APPEND allowedBuildTypes Asan Msan Debug Release RelWithDebInfo)

    IF(NOT CMAKE_BUILD_TYPE)
      SET(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Choose the type of build, options are: ${allowedBuildTypes}" FORCE)
      MESSAGE(WARNING "Implicit build type chosen: ${CMAKE_BUILD_TYPE}")
    ELSEIF(NOT (CMAKE_BUILD_TYPE IN_LIST allowedBuildTypes))
      MESSAGE(FATAL_ERROR "Specified build type: ${CMAKE_BUILD_TYPE} does not match any predefined configuration: ${allowedBuildTypes}")
    ENDIF()

    SET_PROPERTY(CACHE CMAKE_BUILD_TYPE PROPERTY PROPERTY HELPSTRING "Choose the type of build" STRINGS "${allowedBuildTypes}")
endif()

# Define a sanitizer build type based on RelWithDebInfo configuration.
# Sets HIVE_<UPPER_NAME>_COMPILE_OPTIONS and HIVE_<UPPER_NAME>_LINK_OPTIONS
# in parent scope, and creates CMAKE_<flag>_<UPPER_NAME> cache entries.
function(define_sanitizer_build_type name)
    cmake_parse_arguments(ARG "" "" "COMPILE_OPTIONS;LINK_OPTIONS" ${ARGN})

    STRING(TOUPPER "${name}" upperName)
    STRING(TOUPPER "RelWithDebInfo" baseConfig)

    SET(HIVE_${upperName}_COMPILE_OPTIONS ${ARG_COMPILE_OPTIONS} PARENT_SCOPE)
    SET(HIVE_${upperName}_LINK_OPTIONS ${ARG_LINK_OPTIONS} PARENT_SCOPE)

    foreach(flagType IN ITEMS C_FLAGS CXX_FLAGS EXE_LINKER_FLAGS SHARED_LINKER_FLAGS)
        set(CMAKE_${flagType}_${upperName}
            "${CMAKE_${flagType}_${baseConfig}} " CACHE STRING
            "${flagType} for ${name} build type (based on RelWithDebInfo)." FORCE)
        MESSAGE(STATUS "CMAKE_${flagType}_${upperName} based on RelWithDebInfo: ${CMAKE_${flagType}_${baseConfig}}")
    endforeach()
endfunction()

define_sanitizer_build_type(Asan
    COMPILE_OPTIONS -fsanitize=address -fno-omit-frame-pointer -fno-ipa-icf -fno-optimize-sibling-calls
    LINK_OPTIONS -fsanitize=address)

define_sanitizer_build_type(Msan
    COMPILE_OPTIONS -fsanitize=memory -fsanitize-memory-track-origins -fno-omit-frame-pointer -fno-optimize-sibling-calls
    LINK_OPTIONS -fsanitize=memory)
