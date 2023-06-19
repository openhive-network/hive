file(GLOB HEADERS "include/hive/protocol/*.hpp")
file(GLOB HF_PARTS "hardfork.d/*.hf")

if( MSVC )
   set( hardfork_hpp_file "${CMAKE_CURRENT_SOURCE_DIR}/include/hive/protocol/hardfork.hpp" )
   add_custom_target( build_hardfork_hpp
      COMMAND cat-parts "${CMAKE_CURRENT_SOURCE_DIR}/hardfork.d" ${hardfork_hpp_file} )
   add_dependencies( build_hardfork_hpp cat-parts )
else( MSVC )
   set( hardfork_hpp_file "${CMAKE_CURRENT_BINARY_DIR}/include/hive/protocol/hardfork.hpp" )
   add_custom_command(
      COMMAND ${CMAKE_COMMAND} -E env PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/../../programs/build_helpers
      python3 -m hive_build_helpers.cat_parts
      "${CMAKE_CURRENT_SOURCE_DIR}/hardfork.d"
      ${hardfork_hpp_file}
      OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/include/hive/protocol/hardfork.hpp"
      DEPENDS ${HF_PARTS}
   )
endif( MSVC )

include("${CMAKE_CURRENT_SOURCE_DIR}/get_config.d/generate_get_config.cmake")
generate_get_config(
  "${CMAKE_CURRENT_SOURCE_DIR}/include/hive/protocol/config.hpp"  # path to config.hpp
  "${CMAKE_CURRENT_SOURCE_DIR}/get_config.d/get_config.cpp.in"    # path to get_config template file
  "${CMAKE_CURRENT_BINARY_DIR}/get_config.cpp"                    # output path
)

#############

SET( SOURCES_1
   misc_utilities.cpp
   ${HEADERS}
   ${hardfork_hpp_file}
   "${CMAKE_CURRENT_BINARY_DIR}/include/hive/protocol/hardfork.hpp"
)

SET( SOURCES_2
   testnet_blockchain_configuration.cpp
   smt_operations.cpp
   hive_operations.cpp
   dhf_operations.cpp
   types.cpp
   schema_types.cpp
   authority.cpp
   operations.cpp
   sign_state.cpp
   transaction.cpp
   transaction_util.cpp
   block.cpp
   asset.cpp
   version.cpp
   forward_impacted.cpp
   fixed_string.cpp
   ${GET_CONFIG_CPP}
)

set( SOURCES ${SOURCES_1} )
if(NOT WASM_COMPILE_MODE)
  list(APPEND SOURCES ${SOURCES_2})
endif()

cmake_policy(SET CMP0041 NEW)

## SORT .cpp by most likely to change / break compile
add_library( hive_protocol STATIC ${SOURCES} )

if(WASM_COMPILE_MODE)
   target_link_libraries( hive_protocol PUBLIC fc_core )
else()
   target_link_libraries( hive_protocol PUBLIC hive_schema_core fc_core )
endif()

target_link_libraries ( hive_protocol PUBLIC
   $<IF:$<OR:$<STREQUAL:$<TARGET_PROPERTY:TYPE>,SHARED_LIBRARY>,$<STREQUAL:$<TARGET_PROPERTY:TYPE>,MODULE_LIBRARY>>, hive_schema_shared_boost, hive_schema>
)

target_link_libraries ( hive_protocol PUBLIC
$<IF:$<OR:$<STREQUAL:$<TARGET_PROPERTY:TYPE>,SHARED_LIBRARY>,$<STREQUAL:$<TARGET_PROPERTY:TYPE>,MODULE_LIBRARY>>, fc_shared_boost, fc>
)

target_include_directories( hive_protocol
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_BINARY_DIR}/include" )

if( CLANG_TIDY_EXE )
   set_target_properties( hive_protocol PROPERTIES CXX_CLANG_TIDY "${DO_CLANG_TIDY}" )
endif( CLANG_TIDY_EXE )

INSTALL( TARGETS
   hive_protocol

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)

INSTALL( FILES ${HEADERS} DESTINATION "include/hive/protocol" )
