#include <boost/test/unit_test.hpp>

#include <fc/network/http/processor.hpp>
#include <fc/exception/exception.hpp>

BOOST_AUTO_TEST_SUITE( fc_network )

namespace
{
  namespace fh = fc::http;
}

BOOST_AUTO_TEST_CASE( request_method_test )
{
  class request_method_visitor
  {
  public:
    void operator()( const char* name, int64_t index )
    {
      BOOST_REQUIRE( fh::request_method{ name }.get() == static_cast< fh::request_method_type >( index ) );
    }
  };
  request_method_visitor v;
  fc::reflector< fh::request_method_type >::visit( v );
}

BOOST_AUTO_TEST_SUITE_END()
