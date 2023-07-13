file(GLOB HEADERS "../extension/include/extension/*.hpp")
file(GLOB HEADERS "../core/include/core/*.hpp")
file(GLOB HEADERS "include/beekeeper/*.hpp")

ADD_HIVE_EXECUTABLE ( NAME beekeeper
                      SOURCES
                      main.cpp
                      beekeeper_app.cpp
                      beekeeper_instance.cpp
                      session.cpp
                      session_manager.cpp
                      time_manager.cpp
                      ../core/beekeeper_wallet_manager.cpp
                      ../core/beekeeper_wallet_api.cpp
                      ../core/beekeeper_wallet.cpp
                      ../core/token_generator.cpp
                      ../core/time_manager_base.cpp
                      ../core/session_base.cpp
                      ../core/session_manager_base.cpp
                      ../core/wallet_manager_impl.cpp
                      ../core/beekeeper_app_base.cpp
                      ../core/beekeeper_app_init.cpp
                      ../core/utilities.cpp
                      LIBRARIES appbase webserver_plugin hive_utilities
)

target_include_directories( beekeeper PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/../core/include" )

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

