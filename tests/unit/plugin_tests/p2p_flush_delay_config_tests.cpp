#ifdef IS_TEST_NET
#include <boost/test/unit_test.hpp>

#include "../db_fixture/witness_fixture.hpp"

#include <hive/plugins/p2p/p2p_plugin.hpp>

namespace {

struct p2p_flush_delay_fixture : public witness_fixture
{
  /// starts a node with @p extra_config_args and returns the parameters its p2p node runs with
  graphene::net::node_configuration configured_parameters( const config_arg_override_t& extra_config_args )
  {
    BOOST_SCOPE_EXIT( this_ )
    {
      this_->theApp.generate_interrupt_request();
      this_->theApp.wait4interrupt_request();
      this_->theApp.quit( true );
      this_->db = nullptr; // prevent fixture destructor from accessing database after it was closed
    } BOOST_SCOPE_EXIT_END

    initialize( 3, {}, { "initminer" }, extra_config_args );
    return theApp.get_plugin< hive::plugins::p2p::p2p_plugin >().get_advanced_node_parameters();
  }

  int64_t configured_flush_delay( const config_arg_override_t& extra_config_args )
  {
    return configured_parameters( extra_config_args ).transaction_flush_delay_microseconds;
  }

  int64_t configured_fetch_items_minimum_interval( const config_arg_override_t& extra_config_args )
  {
    return configured_parameters( extra_config_args ).fetch_items_minimum_interval_microseconds;
  }
};

} // namespace

BOOST_FIXTURE_TEST_SUITE( p2p_flush_delay_config_tests, p2p_flush_delay_fixture )

BOOST_AUTO_TEST_CASE( flush_delay_defaults_to_1_ms )
{
  try
  {
    // witness_fixture passes p2p-parameters without transaction_flush_delay_microseconds
    BOOST_CHECK_EQUAL( configured_flush_delay( {} ), 1000 );
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

BOOST_AUTO_TEST_CASE( fetch_items_minimum_interval_defaults_to_5_ms )
{
  try
  {
    BOOST_CHECK_EQUAL( configured_fetch_items_minimum_interval( {} ), 5000 );
  }
  FC_LOG_AND_RETHROW()
}

static const std::string p2p_parameters_with_2_ms_fetch_items_minimum_interval =
  R"({"listen_endpoint":"0.0.0.0:0","accept_incoming_connections":false,"fetch_items_minimum_interval_microseconds":2000})";

BOOST_AUTO_TEST_CASE( fetch_items_minimum_interval_p2p_parameter_is_applied )
{
  try
  {
    BOOST_CHECK_EQUAL( configured_fetch_items_minimum_interval(
      { config_line_t( { "p2p-parameters", { p2p_parameters_with_2_ms_fetch_items_minimum_interval } } ) } ), 2000 );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( fetch_items_minimum_interval_option_overrides_p2p_parameters )
{
  try
  {
    BOOST_CHECK_EQUAL( configured_fetch_items_minimum_interval(
      { config_line_t( { "p2p-parameters", { p2p_parameters_with_2_ms_fetch_items_minimum_interval } } ),
        config_line_t( { "p2p-fetch-items-minimum-interval-us", { "0" } } ) } ), 0 );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( maximum_transactions_in_flight_defaults_to_10000 )
{
  try
  {
    BOOST_CHECK_EQUAL( configured_parameters( {} ).maximum_transactions_in_flight, 10000u );
  }
  FC_LOG_AND_RETHROW()
}

static const std::string p2p_parameters_with_50000_transactions_in_flight =
  R"({"listen_endpoint":"0.0.0.0:0","accept_incoming_connections":false,"maximum_transactions_in_flight":50000})";

BOOST_AUTO_TEST_CASE( maximum_transactions_in_flight_p2p_parameter_is_applied )
{
  try
  {
    BOOST_CHECK_EQUAL( configured_parameters(
      { config_line_t( { "p2p-parameters", { p2p_parameters_with_50000_transactions_in_flight } } ) } ).maximum_transactions_in_flight, 50000u );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( maximum_transactions_in_flight_option_overrides_p2p_parameters )
{
  try
  {
    BOOST_CHECK_EQUAL( configured_parameters(
      { config_line_t( { "p2p-parameters", { p2p_parameters_with_50000_transactions_in_flight } } ),
        config_line_t( { "p2p-max-transactions-in-flight", { "100" } } ) } ).maximum_transactions_in_flight, 100u );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()

#endif
