SET( WASM_COMPILE_MODE ON )

ADD_HIVE_EXECUTABLE ( NAME beekeeper_local
                      SOURCES
                      main.cpp
                      beekeeper_local_app.cpp
                      ../core/beekeeper_wallet.cpp
                      ../core/beekeeper_wallet_manager.cpp
                      ../core/beekeper_instance.cpp
                      ../core/token_generator.cpp
                      ../core/time_manager.cpp
                      ../core/session.cpp
                      ../core/session_manager.cpp
                      ../core/wallet_manager_impl.cpp
                      ../core/beekeeper_app_base.cpp
                      ../core/beekeeper_app_init.cpp
                      LIBRARIES appbase
)

target_include_directories( beekeeper_local PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/../core/include" )