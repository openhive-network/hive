file(GLOB HEADERS "include/beekeeper_wasm_core/*.hpp")
file(GLOB HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/../../core/include/beekeeper_wasm_core/*.hpp")

ADD_LIBRARY( beekeeper_wasm_core STATIC
                      beekeeper_wasm_app.cpp
                      ../../core/beekeeper_wallet.cpp
                      ../../core/beekeeper_wallet_manager.cpp
                      ../../core/beekeeper_instance.cpp
                      ../../core/token_generator.cpp
                      ../../core/time_manager.cpp
                      ../../core/session.cpp
                      ../../core/session_manager.cpp
                      ../../core/wallet_manager_impl.cpp
                      ../../core/beekeeper_app_base.cpp
                      ../../core/beekeeper_app_init.cpp
)

TARGET_INCLUDE_DIRECTORIES( beekeeper_wasm_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/../../core/include" )
if( CMAKE_TOOLCHAIN_FILE )
  TARGET_LINK_OPTIONS( beekeeper_wasm_core PUBLIC "-sFORCE_FILESYSTEM" )
endif()
TARGET_LINK_LIBRARIES( beekeeper_wasm_core appbase )
