PROJECT( LiteBeekeeper )
cmake_minimum_required( VERSION 3.22.1 )

set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_CXX_EXTENSIONS OFF )
set( CMAKE_CXX_STANDARD_REQUIRED ON )

set( CMAKE_C_STANDARD 99 )
set( CMAKE_C_STANDARD_REQUIRED ON )

cmake_policy(SET CMP0057 NEW)

file(GLOB HEADERS
      "${CMAKE_CURRENT_SOURCE_DIR}/../beekeeper_wasm/beekeeper_wasm_app_api/include/beekeeper_wasm_app_api/*.hpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/../core/include/beekeeper_wasm_core/*.hpp"
    )

ADD_EXECUTABLE( lite_beekeeper
                      main.cpp
                      ../beekeeper_wasm/beekeeper_wasm_app_api/beekeeper_wasm_app.cpp
                      ../beekeeper_wasm/beekeeper_wasm_app_api/beekeeper_wasm_api.cpp
                      ../core/beekeeper_wallet.cpp
                      ../core/beekeeper_wallet_manager.cpp
                      ../core/token_generator.cpp
                      ../core/time_manager_base.cpp
                      ../core/session_base.cpp
                      ../core/session_manager_base.cpp
                      ../core/wallet_manager_impl.cpp
                      ../core/beekeeper_app_base.cpp
                      ../core/beekeeper_app_init.cpp
)

set( INCLUDES
      ${CMAKE_CURRENT_SOURCE_DIR}/../beekeeper_wasm/beekeeper_wasm_app_api/include
      ${CMAKE_CURRENT_SOURCE_DIR}/../core/include
    )

TARGET_INCLUDE_DIRECTORIES( lite_beekeeper PUBLIC ${INCLUDES} )
target_link_libraries( lite_beekeeper PUBLIC hive_protocol )
