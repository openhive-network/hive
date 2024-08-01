file(GLOB HEADERS "include/core/*.hpp")

add_library( beekeeper_core
             ./wallet_content_handler.cpp
             ./beekeeper_wallet_manager.cpp
             ./token_generator.cpp
             ./session_base.cpp
             ./session_manager_base.cpp
             ./wallet_manager_impl.cpp
             ./beekeeper_app_base.cpp
             ./utilities.cpp
             ./time_manager_base.cpp
             ${HEADERS}
           )

target_link_libraries( beekeeper_core fc )
target_include_directories( beekeeper_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )
target_compile_options( beekeeper_core PUBLIC -fexceptions )
target_link_options( beekeeper_core PUBLIC -fexceptions )

INSTALL( TARGETS
   beekeeper_core

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
INSTALL( FILES ${HEADERS} DESTINATION "include/core" )
