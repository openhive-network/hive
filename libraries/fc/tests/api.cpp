#include <fc/api.hpp>
#include <fc/log/logger.hpp>
#include <fc/rpc/api_connection.hpp>
#include <fc/rpc/websocket_api.hpp>
#include <boost/test/unit_test.hpp>

class calculator
{
   public:
      int32_t add( int32_t a, int32_t b ); // not implemented
      int32_t sub( int32_t a, int32_t b ); // not implemented
      void    on_result( const std::function<void(int32_t)>& cb );
      void    on_result2( const std::function<void(int32_t)>& cb, int test );
};

FC_API( calculator, (add)(sub)(on_result)(on_result2) )


class login_api
{
   public:
      fc::api<calculator> get_calc()const
      {
         FC_ASSERT( calc );
         return *calc;
      }
      fc::optional<fc::api<calculator>> calc;
      std::set<std::string> test( const std::string&, const std::string& ) { return std::set<std::string>(); }
};
FC_API( login_api, (get_calc)(test) );

BOOST_AUTO_TEST_SUITE( fc_api_suite )

using namespace fc;

class some_calculator
{
   public:
      int32_t add( int32_t a, int32_t b ) { wlog("."); if( _cb ) _cb(a+b); return a+b; }
      int32_t sub( int32_t a, int32_t b ) {  wlog(".");if( _cb ) _cb(a-b); return a-b; }
      void    on_result( const std::function<void(int32_t)>& cb ) { wlog( "set callback" ); _cb = cb;  return ; }
      void    on_result2(  const std::function<void(int32_t)>& cb, int test ){}
      std::function<void(int32_t)> _cb;
};
class variant_calculator
{
   public:
      double add( fc::variant a, fc::variant b ) { return a.as_double()+b.as_double(); }
      double sub( fc::variant a, fc::variant b ) { return a.as_double()-b.as_double(); }
      void    on_result( const std::function<void(int32_t)>& cb ) { wlog("set callback"); _cb = cb; return ; }
      void    on_result2(  const std::function<void(int32_t)>& cb, int test ){}
      std::function<void(int32_t)> _cb;
};

using namespace fc::http;
using namespace fc::rpc;

BOOST_AUTO_TEST_CASE( fc_api_test )
{
   some_calculator calc;
   variant_calculator vcalc;

   fc::api<calculator> api_calc( &calc );
   fc::api<calculator> api_vcalc( &vcalc );
   fc::api<calculator> api_nested_calc( api_calc );

   BOOST_REQUIRE( (api_calc->add(5,4)) == 9 );
   BOOST_REQUIRE( (api_calc->sub(5,4)) == 1 );
   BOOST_REQUIRE( (api_vcalc->add(5,4)) == 9 );
   BOOST_REQUIRE( (api_vcalc->sub(5,4)) == 1 );
   BOOST_REQUIRE( (api_nested_calc->sub(5,4)) == 1 );
   BOOST_REQUIRE( (api_nested_calc->sub(5,4)) == 1 );


   // ilog( "------------------ NESTED TEST --------------" );
   BOOST_REQUIRE_NO_THROW(
      login_api napi_impl;
      napi_impl.calc = api_calc;
      fc::api<login_api>  napi(&napi_impl);

      auto client_side = std::make_shared<local_api_connection>();
      auto server_side = std::make_shared<local_api_connection>();
      server_side->set_remote_connection( client_side );
      client_side->set_remote_connection( server_side );

      server_side->register_api( napi );

      fc::api<login_api> remote_api = client_side->get_remote_api<login_api>();

      auto remote_calc = remote_api->get_calc();
      int r = remote_calc->add( 4, 5 );
      BOOST_REQUIRE( r == 9 );
   );
}

BOOST_AUTO_TEST_SUITE_END()
