# Defines Hive library target.
LIST( APPEND CMAKE_MODULE_PATH
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/libraries/fc/CMakeModules"
    "${CMAKE_CURRENT_SOURCE_DIR}/hive/libraries/fc/GitVersionGen" )

#This must be processed before first project or enable_language statement
INCLUDE(hive_build_types)

project( Hive )
cmake_minimum_required( VERSION 3.22.1 )

set( BLOCKCHAIN_NAME "Hive" )

set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_CXX_EXTENSIONS OFF )
set( CMAKE_CXX_STANDARD_REQUIRED ON )

set( CMAKE_C_STANDARD 99 )
set( CMAKE_C_STANDARD_REQUIRED ON )

set( GUI_CLIENT_EXECUTABLE_NAME Hive )
set( CUSTOM_URL_SCHEME "gcs" )
set( INSTALLER_APP_ID "68ad7005-8eee-49c9-95ce-9eed97e5b347" )

set(CMAKE_EXPORT_COMPILE_COMMANDS "ON")
set(CMAKE_VERBOSE_MAKEFILE "ON")

#set (ENABLE_INSTALLER 1)
#set (USE_PCH 1)

if (USE_PCH)
  include (cotire)
endif(USE_PCH)

include( hive_options )
include( hive_targets )

IF( WIN32 )
  SET(BOOST_ROOT $ENV{BOOST_ROOT})
  set(Boost_USE_MULTITHREADED ON)
  set(BOOST_ALL_DYN_LINK OFF) # force dynamic linking for all libraries
ENDIF(WIN32)

cmake_policy(SET CMP0057 NEW)

if( WIN32 )

   message( STATUS "Configuring Hive on WIN32")
   set( DB_VERSION 60 )
   set( BDB_STATIC_LIBS 1 )

   set( ZLIB_LIBRARIES "" )
   SET( DEFAULT_EXECUTABLE_INSTALL_DIR bin/ )

   set(CRYPTO_LIB)

   if( MSVC )
      add_compile_options(/wd4503 /wd4267 /wd4244)
      #looks like this flag can have different default on some machines.
      SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /SAFESEH:NO")
      SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SAFESEH:NO")

      # Probably cmake has a bug and vcxproj generated for executable in Debug conf. has disabled debug info
      set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /DEBUG")

      # On windows tcl should be installed to the directory pointed by setenv.bat script
      SET(TCL_INCLUDE_PATH $ENV{TCL_ROOT}/include)
      MESSAGE(STATUS "tcl INCLUDE PATH: ${TCL_INCLUDE_PATH}")

      FIND_PACKAGE(TCL)
      MESSAGE(STATUS "tcl_library: ${TCL_LIBRARY}")

      SET(TCL_LIBS "optimized;${TCL_LIBRARY};debug;")
      get_filename_component(TCL_LIB_PATH "${TCL_LIBRARY}" PATH)
      get_filename_component(TCL_LIB_NAME "${TCL_LIBRARY}" NAME_WE)
      get_filename_component(TCL_LIB_EXT "${TCL_LIBRARY}" EXT)

      SET(TCL_LIBS "${TCL_LIBS}${TCL_LIB_PATH}/${TCL_LIB_NAME}g${TCL_LIB_EXT}")
      SET(TCL_LIBRARY ${TCL_LIBS})

   elseif( MINGW )
      SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpermissive -msse4.2 -Wa,-mbig-obj")
      SET(CMAKE_CXX_FLAGS_RELEASE "-O3")
      # Optimization flag apparently needed to get rid of "File too big" assembler errors when compiling in Debug mode
      # See: https://stackoverflow.com/questions/14125007/gcc-string-table-overflow-error-during-compilation/14601779#29479701
      SET(CMAKE_CXX_FLAGS_DEBUG "-O2")
      if ( FULL_STATIC_BUILD )
        set( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++ -static-libgcc")
      endif ( FULL_STATIC_BUILD )
   endif( MSVC )

else( WIN32 ) # Apple AND Linux

    find_library(READLINE_LIBRARIES NAMES readline)
    find_path(READLINE_INCLUDE_DIR readline/readline.h)
    #if(NOT READLINE_INCLUDE_DIR OR NOT READLINE_LIBRARIES)
    #  MESSAGE(FATAL_ERROR "Could not find lib readline.")
    #endif()

    if( APPLE )
        # Apple Specific Options Here
        message( STATUS "Configuring Hive on OS X" )
        set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -Wall -Wno-conversion -Wno-deprecated-declarations" )
    else( APPLE )
        # Linux Specific Options Here
        message( STATUS "Configuring Hive on Linux" )
        set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall" )
        set( rt_library rt )
        set( pthread_library pthread)
        if ( NOT DEFINED crypto_library )
          # I'm not sure why this is here, I guess someone has openssl and can't detect it with find_package()?
          # if you have a normal install, you can define crypto_library to the empty string to avoid a build error
          set( crypto_library crypto)
        endif ()
        if ( FULL_STATIC_BUILD )
          set( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++ -static-libgcc")
        endif ( FULL_STATIC_BUILD )
    endif( APPLE )

    if( "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" )
        set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-builtin-memcmp" )
    endif()

    if( "${CMAKE_GENERATOR}" STREQUAL "Ninja" )
        if( "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" )
            set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcolor-diagnostics" )
        endif()
    endif()

    set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")

    # based on http://www.delorie.com/gnu/docs/gdb/gdb_70.html
    # uncomment this line to tell GDB about macros (slows compile times)
    # set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -gdwarf-2 -g3" )

endif( WIN32 )

# external_plugins needs to be compiled first because libraries/app depends on HIVE_EXTERNAL_PLUGINS being fully populated
add_subdirectory( external_plugins )
add_subdirectory( libraries )
add_subdirectory( programs )

add_subdirectory( tests )

if (ENABLE_INSTALLER)

set(VERSION_MAJOR 0)
set(VERSION_MINOR 1)
set(VERSION_PATCH 0)


include(InstallRequiredSystemLibraries)

set(CPACK_OUTPUT_FILE_PREFIX ${CMAKE_BINARY_DIR}/packages)
set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/install)

SET(CPACK_PACKAGE_DIRECTORY "${CMAKE_INSTALL_PREFIX}")
set(CPACK_PACKAGE_NAME "hive")
set(CPACK_PACKAGE_VENDOR "Hive Community")
set(CPACK_PACKAGE_VERSION_MAJOR "${VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${VERSION_PATCH}")
set(CPACK_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
set(CPACK_PACKAGE_DESCRIPTION "A client for the Hive network")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A client for the Hive network")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.md")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Hive ${CPACK_PACKAGE_VERSION}")

if(WIN32)
 SET(CPACK_GENERATOR "ZIP;NSIS")
 set(CPACK_PACKAGE_NAME "Hive") # override above
 set(CPACK_NSIS_EXECUTABLES_DIRECTORY .)
 set(CPACK_NSIS_PACKAGE_NAME "Hive v${CPACK_PACKAGE_VERSION}")
 set(CPACK_NSIS_DISPLAY_NAME "${CPACK_NSIS_PACKAGE_NAME}")
 set(CPACK_NSIS_DEFINES "  !define MUI_STARTMENUPAGE_DEFAULTFOLDER \\\"Hive\\\"")
 # it seems like windows zip files usually don't have a single directory inside them, unix tgz frequently do
 SET(CPACK_INCLUDE_TOPLEVEL_DIRECTORY 0)

endif(WIN32)

if(APPLE)
  set(CPACK_GENERATOR "DragNDrop")
endif()

if(LINUX)
  # Linux gets a .tgz
  SET(CPACK_GENERATOR "TGZ")
  SET(CPACK_INCLUDE_TOPLEVEL_DIRECTORY 1)
endif(LINUX)

 include(CPack)
endif(ENABLE_INSTALLER)

if( BUILD_HIVE_TESTNET )
  MESSAGE( STATUS "\n\n             CONFIGURED FOR TEST NETWORK             \n\n" )
elseif( HIVE_CONVERTER_BUILD )
  MESSAGE( STATUS "\n\n             CONFIGURED FOR ALTERNATE NETWORK             \n\n" )
else()
  MESSAGE( STATUS "\n\n             CONFIGURED FOR HIVE NETWORK             \n\n" )
endif()

if( ENABLE_SMT_SUPPORT )
MESSAGE( STATUS "\n\n             CONFIGURED FOR SMT SUPPORT             \n\n" )
else()
MESSAGE( STATUS "\n\n             CONFIGURED FOR NO SUPPORT OF SMT             \n\n" )
endif()

set(INDENT_WIDTH 2 CACHE STRING "Determine how many spaces should be used for formating (by default 2)")
find_program(PYTHON_3_6 "python3.6")

# How to launch?
# cmake --build . --target format
if(PYTHON_3_6)
  add_custom_target(format COMMAND ${PYTHON_3_6} ${CMAKE_SOURCE_DIR}/format.py ${INDENT_WIDTH} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  MESSAGE( STATUS "\n\n             FORMAT IS AVAILABLE                \n\n" )
else(PYTHON_3_6)
  MESSAGE( STATUS "\n\n             FORMAT IS NOT AVAILABLE                \n\n" )
endif( PYTHON_3_6 )
