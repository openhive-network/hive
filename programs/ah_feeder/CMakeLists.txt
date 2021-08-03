
add_executable( ah_feeder main.cpp )

if( UNIX AND NOT APPLE )
  set(rt_library rt )
endif()

find_package( Gperftools QUIET )
if( GPERFTOOLS_FOUND )
    message( STATUS "Found gperftools; compiling ah_feeder with TCMalloc")
    list( APPEND PLATFORM_SPECIFIC_LIBS tcmalloc )
endif()

if( HIVE_STATIC_BUILD )
   target_link_libraries( ah_feeder PRIVATE
      "-static-libstdc++ -static-libgcc"
      hive_protocol
      hive_utilities
      hive_chain
      ${CMAKE_DL_LIBS}hive_apphive_app
      ${PLATFORM_SPECIFIC_LIBS}
      ${BROTLI_LIBRARIES}
   )
else( HIVE_STATIC_BUILD )
   target_link_libraries( ah_feeder PRIVATE
      hive_protocol
      hive_utilities
      hive_chain
      ${CMAKE_DL_LIBS}
      ${PLATFORM_SPECIFIC_LIBS}
      ${BROTLI_LIBRARIES}
   )
endif( HIVE_STATIC_BUILD )

if( CLANG_TIDY_EXE )
   set_target_properties(
    ah_feeder PROPERTIES
      CXX_CLANG_TIDY "${DO_CLANG_TIDY}"
   )
endif( CLANG_TIDY_EXE )

install( TARGETS
ah_feeder
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
