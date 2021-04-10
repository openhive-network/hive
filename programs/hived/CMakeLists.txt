add_executable( hived main.cpp )
if( UNIX AND NOT APPLE )
  set(rt_library rt )
endif()

find_package( Gperftools QUIET )
if( GPERFTOOLS_FOUND )
    message( STATUS "Found gperftools; compiling hived with TCMalloc")
    list( APPEND PLATFORM_SPECIFIC_LIBS tcmalloc )
endif()

if( HIVE_STATIC_BUILD )
   target_link_libraries( hived PRIVATE
      "-static-libstdc++ -static-libgcc"
      appbase
      hive_utilities
      hive_plugins
      ${CMAKE_DL_LIBS}
      ${PLATFORM_SPECIFIC_LIBS}
      ${BROTLI_LIBRARIES}
   )
else( HIVE_STATIC_BUILD )
   target_link_libraries( hived PRIVATE
      appbase
      hive_utilities
      hive_plugins
      ${CMAKE_DL_LIBS}
      ${PLATFORM_SPECIFIC_LIBS}
      ${BROTLI_LIBRARIES}
   )
endif( HIVE_STATIC_BUILD )

if( CLANG_TIDY_EXE )
   set_target_properties(
      hived PROPERTIES
      CXX_CLANG_TIDY "${DO_CLANG_TIDY}"
   )
endif( CLANG_TIDY_EXE )

install( TARGETS
   hived

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
