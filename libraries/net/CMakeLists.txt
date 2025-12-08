file(GLOB HEADERS "include/graphene/net/*.hpp")

set(SOURCES node.cpp
            stcp_socket.cpp
            core_messages.cpp
            peer_database.cpp
            peer_connection.cpp
            message_oriented_connection.cpp)

add_library( graphene_net ${SOURCES} ${HEADERS} )

# Tests
add_executable(message_serialization_test tests/message_serialization_tests.cpp)
target_link_libraries(message_serialization_test graphene_net hive_chain fc ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY})
target_include_directories(message_serialization_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
add_test(NAME message_serialization_test COMMAND message_serialization_test)

target_link_libraries( graphene_net
  PUBLIC hive_protocol statsd_plugin fc )
target_include_directories( graphene_net
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_BINARY_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/../chain/include"
)

if(MSVC)
  set_source_files_properties( node.cpp PROPERTIES COMPILE_FLAGS "/bigobj" )
endif(MSVC)

if (USE_PCH)
  set_target_properties(graphene_net PROPERTIES COTIRE_ADD_UNITY_BUILD FALSE)
  cotire(graphene_net)
endif(USE_PCH)

if( CLANG_TIDY_EXE )
   set_target_properties(
      graphene_net PROPERTIES
      CXX_CLANG_TIDY "${DO_CLANG_TIDY}"
   )
endif( CLANG_TIDY_EXE )

install( TARGETS
   graphene_net

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
install( FILES ${HEADERS} DESTINATION "include/graphene/net" )
