
file(GLOB HEADERS "include/beekeeper_wasm_core/*.hpp" "${CMAKE_CURRENT_SOURCE_DIR}/../../core/include/beekeeper_wasm_core/*.hpp" )

add_subdirectory( ${CMAKE_CURRENT_SOURCE_DIR}/../../../../libraries/fc_minimal build_2 )

ADD_LIBRARY( beekeeper_wasm_core STATIC
                      beekeeper_wasm_app.cpp
                      ../../core/beekeeper_wallet.cpp
                      ../../core/beekeeper_wallet_manager.cpp
                      ../../core/token_generator.cpp
                      ../../core/time_manager.cpp
                      ../../core/session_base.cpp
                      ../../core/session_manager_base.cpp
                      ../../core/wallet_manager_impl.cpp
                      ../../core/beekeeper_app_base.cpp
                      ../../core/beekeeper_app_init.cpp
)

set( INCLUDES
      ${CMAKE_CURRENT_SOURCE_DIR}/include
      ${CMAKE_CURRENT_SOURCE_DIR}/../../core/include
    )

TARGET_INCLUDE_DIRECTORIES( beekeeper_wasm_core PUBLIC ${INCLUDES} )
TARGET_LINK_LIBRARIES( beekeeper_wasm_core fc_minimal hive_protocol )