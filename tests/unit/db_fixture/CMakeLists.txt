file(GLOB HEADERS "database_fixture.hpp")
set(SOURCES database_fixture.cpp
            hived_fixture.cpp
            clean_database_fixture.cpp)

find_package( Gperftools QUIET )
if( GPERFTOOLS_FOUND )
    message( STATUS "Found gperftools; compiling tests with TCMalloc")
    list( APPEND PLATFORM_SPECIFIC_LIBS tcmalloc )
endif()

# Static in the shared variant too: the fixtures use Boost.Test, whose static library each test executable links
# itself, so the fixtures must share that executable's copy rather than carry one of their own.
add_library(db_fixture STATIC
            ${SOURCES}
            ${HEADERS})

target_link_libraries(db_fixture
                      hive_chain
                      hive_protocol
                      account_history_rocksdb_plugin
                      market_history_plugin
                      block_api_plugin
                      database_api_plugin
                      webserver_plugin
                      witness_plugin
                      debug_node_plugin
                      condenser_api_plugin
                      fc
                      hive_plugins
                      ${PLATFORM_SPECIFIC_LIBS})
