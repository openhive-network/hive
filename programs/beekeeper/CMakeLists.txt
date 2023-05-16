file(GLOB HEADERS "include/beekeeper/*.hpp")

ADD_HIVE_EXECUTABLE ( NAME beekeeper
                      SOURCES
                      main.cpp
                      beekeeper_wallet_api.cpp
                      beekeeper_wallet.cpp
                      beekeeper_wallet_manager.cpp
                      singleton_beekeeper.cpp
                      LIBRARIES appbase webserver_plugin hive_utilities
)

target_include_directories( beekeeper PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

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

