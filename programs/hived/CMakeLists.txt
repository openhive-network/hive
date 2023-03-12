ADD_HIVE_EXECUTABLE ( NAME hived
                      SOURCES main.cpp
                      LIBRARIES appbase hive_utilities hive_plugins
)

if( UNIX AND NOT APPLE )
  set(rt_library rt )
endif()

# https://stackoverflow.com/a/40922025/11738218
OPTION(ENABLE_PROFILING "If set additional flags are passed to compiler" OFF)
if(ENABLE_PROFILING)
    SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg" )
    SET( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pg" )
    SET( CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -pg" )
    message( INFO "Profiling enabled" )
endif()

