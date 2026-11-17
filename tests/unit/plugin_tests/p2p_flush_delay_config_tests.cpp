#ifdef IS_TEST_NET
#include <boost/test/unit_test.hpp>

#include "../db_fixture/witness_fixture.hpp"

#include <hive/plugins/p2p/p2p_plugin.hpp>

namespace {

struct p2p_flush_delay_fixture : public witness_fixture
{
  /// starts a node with @p extra_config_args and returns the transaction flush delay its p2p node runs with
  int64_t configured_flush_delay( const config_arg_override_t& extra_config_args )
  {
    BOOST_SCOPE_EXIT( this_ )
    {
      this_->theApp.generate_interrupt_request();
      this_->theApp.wait4interrupt_request();
      this_->theApp.quit( true );
      this_->db = nullptr; // prevent fixture destructor from accessing database after it was closed
    } BOOST_SCOPE_EXIT_END

    initialize( 3, {}, { "initminer" }, extra_config_args );
    return theApp.get_plugin< hive::plugins::p2p::p2p_plugin >().get_advanced_node_parameters().transaction_flush_delay_microseconds;
  }
};

} // namespace

BOOST_FIXTURE_TEST_SUITE( p2p_flush_delay_config_tests, p2p_flush_delay_fixture )

BOOST_AUTO_TEST_CASE( flush_delay_defaults_to_5_ms )
{
  try
  {
    // witness_fixture passes p2p-parameters without transaction_flush_delay_microseconds
    BOOST_CHECK_EQUAL( configured_flush_delay( {} ), 5000 );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( flush_delay_option_zero_disables_the_delay )
{
  try
  {
    BOOST_CHECK_EQUAL( configured_flush_delay( { config_line_t( { "p2p-transaction-flush-delay-us", { "0" } } ) } ), 0 );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( flush_delay_p2p_parameter_zero_disables_the_delay )
{
  try
  {
    const std::string p2p_parameters =
      R"({"listen_endpoint":"0.0.0.0:0","accept_incoming_connections":false,"transaction_flush_delay_microseconds":0})";
    BOOST_CHECK_EQUAL( configured_flush_delay( { config_line_t( { "p2p-parameters", { p2p_parameters } } ) } ), 0 );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()

#endif
