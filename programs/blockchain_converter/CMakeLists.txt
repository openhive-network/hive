if( NOT HIVE_CONVERTER_BUILD )

  message( AUTHOR_WARNING "Building without hive converter" )

else()

  add_executable( blockchain_converter main.cpp converter.cpp conversion_plugin.cpp block_log_conversion_plugin.cpp node_based_conversion_plugin.cpp )

  FIND_PACKAGE(Boost 1.58 REQUIRED COMPONENTS atomic )

  if( HIVE_STATIC_BUILD )
    target_link_libraries( blockchain_converter PRIVATE
                          "-static-libstdc++ -static-libgcc"
                          hive_utilities hive_protocol hive_chain condenser_api_plugin appbase fc ${CMAKE_DL_LIBS} ${PLATFORM_SPECIFIC_LIBS}
    )
  else( HIVE_STATIC_BUILD )
    target_link_libraries( blockchain_converter PRIVATE
                          hive_utilities hive_protocol hive_chain condenser_api_plugin appbase fc ${CMAKE_DL_LIBS} ${PLATFORM_SPECIFIC_LIBS}
    )
  endif( HIVE_STATIC_BUILD )

  if( CLANG_TIDY_EXE )
    set_target_properties(
      blockchain_converter PROPERTIES
      CXX_CLANG_TIDY "${DO_CLANG_TIDY}"
    )
  endif( CLANG_TIDY_EXE )

  install( TARGETS
  blockchain_converter

    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
  )

endif()
