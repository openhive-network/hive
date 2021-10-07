#include <boost/test/unit_test.hpp>

#include <fc/network/http/processors/http_unsupported.hpp>
#include <fc/network/http/processor.hpp>
#include <fc/exception/exception.hpp>

BOOST_AUTO_TEST_SUITE( fc_network )

BOOST_AUTO_TEST_CASE( processors_http_unsupported_test )
{
  namespace fh = fc::http;
  fh::version tver = fh::version::http_unsupported; // Testing version

  // BOOST_REQUIRE( fh::processor::has_processor_for( tver ) ); // Unsupported http version processor is an "invalid" processor, so this check will always fail

  fh::processor_ptr proc = fh::processor::get_for_version( tver );

}

BOOST_AUTO_TEST_SUITE_END()
