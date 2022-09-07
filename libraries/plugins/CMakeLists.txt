# for each subdirectory containing a CMakeLists.txt, add that subdirectory
file( GLOB children RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} * )
foreach( child ${children} )
   if( IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${child}" OR IS_SYMLINK "${CMAKE_CURRENT_SOURCE_DIR}/${child}" )
      if( EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${child}/CMakeLists.txt" )
         add_subdirectory( "${CMAKE_CURRENT_SOURCE_DIR}/${child}" )
      endif()
   endif()
endforeach()
