file(GLOB HEADERS
      "${CMAKE_CURRENT_SOURCE_DIR}/../../core/include/beekeeper_wasm_core/*.hpp"
    )

include( ${CMAKE_CURRENT_SOURCE_DIR}/../../../../cmake/hive_targets.cmake )

set(BOOST_COMPONENTS)
LIST(APPEND BOOST_COMPONENTS
      atomic
      chrono
      date_time
      filesystem
      program_options
      system)
set(HIVE_BUILD_ON_MINIMAL_FC ON)
add_subdirectory( ${CMAKE_CURRENT_SOURCE_DIR}/../../../../libraries/fc build_fc_minimal )

add_subdirectory( ${CMAKE_CURRENT_SOURCE_DIR}/../../../../libraries/schema build_schema_minimal )
add_subdirectory( ${CMAKE_CURRENT_SOURCE_DIR}/../../../../libraries/protocol build_protocol_minimal )

ADD_LIBRARY( beekeeper_wasm_core STATIC
                      ../../core/beekeeper_wasm_app.cpp
                      ../../core/beekeeper_wasm_api.cpp
                      ../../core/beekeeper_wallet.cpp
                      ../../core/beekeeper_wallet_manager.cpp
                      ../../core/token_generator.cpp
                      ../../core/time_manager_base.cpp
                      ../../core/session_base.cpp
                      ../../core/session_manager_base.cpp
                      ../../core/wallet_manager_impl.cpp
                      ../../core/beekeeper_app_base.cpp
                      ../../core/beekeeper_app_init.cpp
                      ../../core/utilities.cpp
)

set( INCLUDES
      ${CMAKE_CURRENT_SOURCE_DIR}/../../core/include
    )

TARGET_INCLUDE_DIRECTORIES( beekeeper_wasm_core PUBLIC ${INCLUDES} )
TARGET_LINK_LIBRARIES( beekeeper_wasm_core hive_protocol )