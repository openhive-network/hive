MACRO( ADD_TARGET_BOOST_LIBRARIES target_name )
    SET( BOOST_COMPONENTS )
    # Here define all boost libraries being used by Hive subprojects
    LIST( APPEND BOOST_COMPONENTS
            atomic
            chrono
            context
            coroutine
            date_time
            filesystem
            iostreams
            locale
            system
            thread
            program_options
            unit_test_framework
    )

    LIST ( TRANSFORM BOOST_COMPONENTS PREPEND "Boost::" OUTPUT_VARIABLE _boost_implicit_targets )

    MESSAGE ( STATUS ${_boost_implicit_targets} )

    SET( Boost_NO_BOOST_CMAKE ON CACHE STRING "ON or OFF" FORCE )

    SET ( ORIGINAL_LIB_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES} )
    SET ( ORIGINAL_Boost_USE_STATIC_LIBS ${Boost_USE_STATIC_LIBS} )

    GET_TARGET_PROPERTY(_target_type ${target_name} TYPE)

    MESSAGE( STATUS "Setting up Boost libraries for target: ${target_name} defined as: ${_target_type}" )

    IF (_target_type  STREQUAL "EXECUTABLE")
      # Executable can always link against static Boost libraries, to reduce binary dependencies
      SET( Boost_USE_STATIC_LIBS ON CACHE STRING "ON or OFF" FORCE )
      SET( CMAKE_FIND_LIBRARY_SUFFIXES ".a")

    ELSEIF (_target_type  STREQUAL "SHARED_LIBRARY")
      # Shared library shall always link against shared Boost libraries
      SET( Boost_USE_STATIC_LIBS OFF CACHE STRING "ON or OFF" FORCE )
      SET( CMAKE_FIND_LIBRARY_SUFFIXES ".so")

    ELSE() 
      # Nothing to do for static libraries - they does not need to know dependencies at library level, since all symbols must be finally solved at .so or exec level
      #FIND_PACKAGE( Boost 1.74 REQUIRED COMPONENTS ${BOOST_COMPONENTS} )
      GET_TARGET_PROPERTY(_target_PIC ${target_name} POSITION_INDEPENDENT_CODE)

      IF ( ${_target_PIC} )
        MESSAGE( STATUS "Target: ${target_name} requires boost dynamic linkage" )
        SET( Boost_USE_STATIC_LIBS OFF CACHE STRING "ON or OFF" FORCE )
        SET( CMAKE_FIND_LIBRARY_SUFFIXES ".so")
      ELSE()
        MESSAGE( STATUS "Target: ${target_name} can use boost static linkage" )
        SET( Boost_USE_STATIC_LIBS ON CACHE STRING "ON or OFF" FORCE )
        SET( CMAKE_FIND_LIBRARY_SUFFIXES ".a")
      ENDIF()

    ENDIF ()

    FIND_PACKAGE( Boost 1.74 REQUIRED COMPONENTS ${BOOST_COMPONENTS} )

    TARGET_LINK_LIBRARIES( ${target_name} INTERFACE ${Boost_LIBRARIES} )

    MESSAGE ( STATUS "Detected boost libs: ${Boost_LIBRARIES}" )

    SET( CMAKE_FIND_LIBRARY_SUFFIXES ${ORIGINAL_LIB_SUFFIXES} )
    SET( Boost_USE_STATIC_LIBS ${ORIGINAL_Boost_USE_STATIC_LIBS} )

    TARGET_INCLUDE_DIRECTORIES( ${target_name} INTERFACE ${Boost_INCLUDE_DIR} )

ENDMACRO()

MACRO( ADD_TARGET_OPENSSL_LIBRARIES target_name use_static_libs )

  IF ( ${use_static_libs} )
    MESSAGE( STATUS "Setting up OpenSSL STATIC libraries for target: ${target_name}" )
    SET( OPENSSL_USE_STATIC_LIBS TRUE )
  ELSE() 
    MESSAGE( STATUS "Setting up OpenSSL SHARED libraries for target: ${target_name}" )
  ENDIF ()

  FIND_PACKAGE( OpenSSL REQUIRED )

  TARGET_LINK_LIBRARIES( ${target_name} INTERFACE ${OPENSSL_LIBRARIES} )

  MESSAGE ( STATUS "Detected OpenSSL libs: ${OPENSSL_LIBRARIES}" )

  UNSET ( OPENSSL_USE_STATIC_LIBS )
  UNSET (OPENSSL_DIR CACHE  )
  UNSET (OPENSSL_LIBRARIES CACHE)
  UNSET (OPENSSL_VERSION CACHE)
  UNSET (OPENSSL_CRYPTO_LIBRARY CACHE )
  UNSET (OPENSSL_SSL_LIBRARY CACHE )
  UNSET (OPENSSL_FOUND CACHE )

ENDMACRO()

# Allows to define Hive project specific executable target.
# By default, this target depends on Boost libraries defined in ADD_TARGET_BOOST_LIBRARIES.
# In very rare cases when this is not needed, linker will skip such libraries...

MACRO( ADD_HIVE_EXECUTABLE)
  SET(multiValueArgs SOURCES LIBRARIES)
  SET(OPTIONS "")
  SET(oneValueArgs NAME )

  CMAKE_PARSE_ARGUMENTS( HIVE_EXE "${OPTIONS}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN} )

  ADD_EXECUTABLE( ${HIVE_EXE_NAME} ${HIVE_EXE_SOURCES} )

  FIND_PACKAGE( Gperftools QUIET )
  IF( GPERFTOOLS_FOUND )
    MESSAGE( STATUS "Found gperftools; compiling ${HIVE_EXE_NAME} with TCMalloc")
    LIST( APPEND PLATFORM_SPECIFIC_LIBS tcmalloc )
  endif()

  TARGET_LINK_LIBRARIES( ${HIVE_EXE_NAME} PUBLIC
     "-static-libstdc++ -static-libgcc"

     ${HIVE_EXE_LIBRARIES}

#     ${CMAKE_DL_LIBS}
#     ${PLATFORM_SPECIFIC_LIBS}
    )

  set(_libs )

  GET_TARGET_PROPERTY(_libs ${HIVE_EXE_NAME} LINK_LIBRARIES)

  MESSAGE ( STATUS "Target libraries before adding boost: ${_libs} " )



  #ADD_TARGET_BOOST_LIBRARIES( ${HIVE_EXE_NAME} )

  

  set(_libs )

  GET_TARGET_PROPERTY(_libs ${HIVE_EXE_NAME} LINK_LIBRARIES)

  MESSAGE ( STATUS "Target libraries after adding boost: ${_libs} " )

  # needed to correctly print crash stacktrace
  SET_TARGET_PROPERTIES( ${HIVE_EXE_NAME} PROPERTIES ENABLE_EXPORTS true)

  IF( CLANG_TIDY_EXE )
    SET_TARGET_PROPERTIES( ${HIVE_EXE_NAME} PROPERTIES CXX_CLANG_TIDY "${DO_CLANG_TIDY}" )
  ENDIF()

  INSTALL( TARGETS ${HIVE_EXE_NAME}
     RUNTIME DESTINATION bin
     LIBRARY DESTINATION lib
     ARCHIVE DESTINATION lib
  )

ENDMACRO()
