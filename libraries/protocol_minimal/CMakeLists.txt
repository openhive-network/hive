SET( protocol_dir "../protocol/")

file(GLOB HEADERS "${protocol_dir}/include/hive/protocol/*.hpp")
file(GLOB HF_PARTS "${protocol_dir}/hardfork.d/*.hf")

if( MSVC )
   set( hardfork_hpp_file "${CMAKE_CURRENT_SOURCE_DIR}/${protocol_dir}/include/hive/protocol/hardfork.hpp" )
   add_custom_target( build_hardfork_hpp
      COMMAND cat-parts "${CMAKE_CURRENT_SOURCE_DIR}/${protocol_dir}/hardfork.d" ${hardfork_hpp_file} )
   add_dependencies( build_hardfork_hpp cat-parts )
else( MSVC )
   set( hardfork_hpp_file "${CMAKE_CURRENT_BINARY_DIR}/include/hive/protocol/hardfork.hpp" )
   add_custom_command(
      COMMAND ${CMAKE_COMMAND} -E env PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/${protocol_dir}/../../programs/build_helpers
      python3 -m hive_build_helpers.cat_parts
      "${CMAKE_CURRENT_SOURCE_DIR}/${protocol_dir}/hardfork.d"
      ${hardfork_hpp_file}
      OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/include/hive/protocol/hardfork.hpp"
      DEPENDS ${HF_PARTS}
   )
endif( MSVC )

include("${CMAKE_CURRENT_SOURCE_DIR}/${protocol_dir}/get_config.d/generate_get_config.cmake")
generate_get_config(
  "${CMAKE_CURRENT_SOURCE_DIR}/${protocol_dir}/include/hive/protocol/config.hpp"  # path to config.hpp
  "${CMAKE_CURRENT_SOURCE_DIR}/${protocol_dir}/get_config.d/get_config.cpp.in"    # path to get_config template file
  "${CMAKE_CURRENT_BINARY_DIR}/get_config.cpp"                    # output path
)

#############

SET( SOURCES ${protocol_dir}/testnet_blockchain_configuration.cpp
   ${protocol_dir}/smt_operations.cpp
   ${protocol_dir}/hive_operations.cpp
   ${protocol_dir}/dhf_operations.cpp
   ${protocol_dir}/types.cpp
   ${protocol_dir}/schema_types.cpp
   ${protocol_dir}/authority.cpp
   ${protocol_dir}/operations.cpp
   ${protocol_dir}/sign_state.cpp
   ${protocol_dir}/transaction.cpp
   ${protocol_dir}/transaction_util.cpp
   ${protocol_dir}/block.cpp
   ${protocol_dir}/asset.cpp
   ${protocol_dir}/version.cpp
   ${protocol_dir}/misc_utilities.cpp
   ${protocol_dir}/forward_impacted.cpp
   ${protocol_dir}/fixed_string.cpp
   ${GET_CONFIG_CPP}
   ${HEADERS}
   ${hardfork_hpp_file}
   "${CMAKE_CURRENT_BINARY_DIR}/include/hive/protocol/hardfork.hpp"
)

cmake_policy(SET CMP0041 NEW)

## SORT .cpp by most likely to change / break compile

add_library( hive_protocol_minimal STATIC ${SOURCES} )

target_link_libraries( hive_protocol_minimal PUBLIC hive_schema_minimal fc_minimal )

target_include_directories( hive_protocol_minimal
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/${protocol_dir}/include" "${CMAKE_CURRENT_BINARY_DIR}/include" )

if( CLANG_TIDY_EXE )
   set_target_properties( hive_protocol PROPERTIES CXX_CLANG_TIDY "${DO_CLANG_TIDY}" )
endif( CLANG_TIDY_EXE )
