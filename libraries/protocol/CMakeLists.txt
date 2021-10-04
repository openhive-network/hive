file(GLOB HEADERS "include/hive/protocol/*.hpp")
file(GLOB HF_PARTS "hardfork.d/*.hpp")

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

## SORT .cpp by most likely to change / break compile
add_library( hive_protocol

             testnet_blockchain_configuration.cpp
             smt_operations.cpp
             hive_operations.cpp
             sps_operations.cpp
             hive_required_actions.cpp
             hive_optional_actions.cpp
             types.cpp
             schema_types.cpp
             authority.cpp
             operations.cpp
             sign_state.cpp
             transaction.cpp
             block.cpp
             asset.cpp
             version.cpp
             misc_utilities.cpp
             forward_impacted.cpp

             ${GET_CONFIG_CPP}

             ${HEADERS}
             ${hardfork_hpp_file}
             "${CMAKE_CURRENT_BINARY_DIR}/include/hive/protocol/hardfork.hpp"
           )

target_link_libraries( hive_protocol hive_schema fc )
target_include_directories( hive_protocol
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_BINARY_DIR}/include" )

if( CLANG_TIDY_EXE )
   set_target_properties(
      hive_protocol PROPERTIES
      CXX_CLANG_TIDY "${DO_CLANG_TIDY}"
   )
endif( CLANG_TIDY_EXE )

INSTALL( TARGETS
   hive_protocol

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)

INSTALL( FILES ${HEADERS} DESTINATION "include/hive/protocol" )
