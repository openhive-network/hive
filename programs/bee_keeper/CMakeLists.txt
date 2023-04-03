ADD_HIVE_EXECUTABLE ( NAME bee_keeper
                      SOURCES main.cpp
                      LIBRARIES appbase webserver_plugin clive_api_plugin clive_plugin
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

