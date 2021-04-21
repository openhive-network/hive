# - Try to find readline include dirs and libraries 
#
# Usage of this module as follows:
#
#     find_package(Readline)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#  Readline_ROOT_DIR         Set this variable to the root installation of
#                            readline if the module has problems finding the
#                            proper installation path.
#  Readline_USE_STATIC_LIBS  Set to ON to force the use of the static
#                            libraries. Default is OFF.
#
# Variables defined by this module:
#
#  READLINE_FOUND            System has readline, include and lib dirs found
#  Readline_INCLUDE_DIR      The readline include directories.
#  Readline_LIBRARIES        The readline libraries.

find_path(Readline_ROOT_DIR
    NAMES include/readline/readline.h
)

find_path(Readline_INCLUDE_DIR
    NAMES readline/readline.h
    HINTS ${Readline_ROOT_DIR}/include
)

set( _Readline_LIBRARIES readline )
if( Readline_USE_STATIC_LIBS )
  set( _Readline_LIBRARIES libreadline.a ${_Readline_LIBRARIES} )
endif()

find_library(Readline_LIBRARIES
    NAMES ${_Readline_LIBRARIES}
    HINTS ${Readline_ROOT_DIR}/lib
)

if( Readline_USE_STATIC_LIBS )
  find_library( Tinfo_LIBRARY NAMES libtinfo.a )
  list( APPEND Readline_LIBRARIES ${Readline_LIBRARIES} ${Tinfo_LIBRARY} )
endif()

if(Readline_INCLUDE_DIR AND Readline_LIBRARIES AND Ncurses_LIBRARY)
  set(READLINE_FOUND TRUE)
else(Readline_INCLUDE_DIR AND Readline_LIBRARIES AND Ncurses_LIBRARY)
  FIND_LIBRARY(Readline_LIBRARIES NAMES readline)
  include(FindPackageHandleStandardArgs)
  FIND_PACKAGE_HANDLE_STANDARD_ARGS(Readline DEFAULT_MSG Readline_INCLUDE_DIR Readline_LIBRARIES )
  MARK_AS_ADVANCED(Readline_INCLUDE_DIR Readline_LIBRARIES)
endif(Readline_INCLUDE_DIR AND Readline_LIBRARIES AND Ncurses_LIBRARY)

mark_as_advanced(
    Readline_ROOT_DIR
    Readline_INCLUDE_DIR
    Readline_LIBRARIES
)

MESSAGE( STATUS "Found Readline: ${Readline_LIBRARIES}" )
