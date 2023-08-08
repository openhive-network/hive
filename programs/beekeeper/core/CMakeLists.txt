file(GLOB HEADERS "include/core/*.hpp")

add_library( beekeeper_core
             ./beekeeper_wallet.cpp
             ./beekeeper_wallet_manager.cpp
             ./token_generator.cpp
             ./session_base.cpp
             ./session_manager_base.cpp
             ./wallet_manager_impl.cpp
             ./beekeeper_app_base.cpp
             ./beekeeper_app_init.cpp
             ./utilities.cpp
             ./time_manager_base.cpp
             ${HEADERS}
           )

target_link_libraries( beekeeper_core hive_protocol )
target_include_directories( beekeeper_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

INSTALL( TARGETS
   beekeeper_core

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
INSTALL( FILES ${HEADERS} DESTINATION "include/core" )
