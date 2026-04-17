add_executable( chainbase_test test.cpp )
target_link_libraries( chainbase_test chainbase ${PLATFORM_SPECIFIC_LIBS} )

# Standalone test for pool_allocator_t in std::allocator mode.
# Compiled with -DENABLE_STD_ALLOCATOR to exercise the code path that uses
# std::allocator, std::set, std::vector, and raw pointers instead of
# boost::interprocess equivalents.
# Uses header-only Boost.Test (included variant) to avoid static/shared
# library mismatch across build environments.
add_executable( std_allocator_test test_std_allocator.cpp )
target_compile_definitions( std_allocator_test PRIVATE ENABLE_STD_ALLOCATOR )
target_include_directories( std_allocator_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include" )
target_link_libraries( std_allocator_test ${PLATFORM_SPECIFIC_LIBS} )
