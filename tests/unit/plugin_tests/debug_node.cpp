#if defined IS_TEST_NET && !defined ENABLE_STD_ALLOCATOR
#include <boost/test/unit_test.hpp>

#include "../db_fixture/clean_database_fixture.hpp"

using namespace hive::chain;
using namespace hive::protocol;
using namespace hive::plugins;

BOOST_FIXTURE_TEST_SUITE( debug_node, clean_database_fixture )

BOOST_AUTO_TEST_CASE( vests_hive_evaluation )
{
  auto _calculate_modifiers = [ this ]( share_type price_hive_base, share_type price_vests_quote,
                                        share_type total_vested_hive, share_type total_vests,
                                        share_type hive_modifier, share_type vest_modifier
                                )
  {
    hive::protocol::HIVE_asset _total_vested_hive( total_vested_hive );
    hive::protocol::VEST_asset _total_vests( total_vests );
    hive::protocol::HIVE_asset _hive_modifier;
    hive::protocol::VEST_asset _vest_modifier;
    hive::protocol::price _new_price   = hive::protocol::price( hive::protocol::asset( price_hive_base, HIVE_SYMBOL ), hive::protocol::asset( price_vests_quote, VESTS_SYMBOL ) );

    db_plugin->calculate_modifiers_according_to_new_price( _new_price, _total_vested_hive, _total_vests, _hive_modifier, _vest_modifier );

    char _buffer[500];
    sprintf( _buffer, "_hive_modifier: ( %ld == %ld ) _vest_modifier: ( %ld == %ld )", _hive_modifier.amount.value, hive_modifier.value, _vest_modifier.amount.value, vest_modifier.value );
    BOOST_TEST_MESSAGE( _buffer );
    BOOST_CHECK( _hive_modifier == hive::protocol::HIVE_asset( hive_modifier ) );
    BOOST_CHECK( _vest_modifier == hive::protocol::VEST_asset( vest_modifier ) );
  };

  _calculate_modifiers( 1/*price_hive_base*/, 500/*price_vests_quote*/, 13'044/*total_vested_hive*/, 12'863'762'116'631/*total_vests*/, 25'727'511'189/*hive_modifier*/, 0/*vest_modifier*/ );

  _calculate_modifiers( 1/*price_hive_base*/, 1/*price_vests_quote*/, 1'000/*total_vested_hive*/, 1'000/*total_vests*/, 0/*hive_modifier*/, 0/*vest_modifier*/ );

  _calculate_modifiers( 1/*price_hive_base*/, 2/*price_vests_quote*/, 1'000/*total_vested_hive*/, 1'000/*total_vests*/, 0/*hive_modifier*/, 1'000/*vest_modifier*/ );
  _calculate_modifiers( 2/*price_hive_base*/, 1/*price_vests_quote*/, 1'000/*total_vested_hive*/, 1'000/*total_vests*/, 1'000/*hive_modifier*/, 0/*vest_modifier*/ );

  _calculate_modifiers( 1/*price_hive_base*/, 1'000/*price_vests_quote*/, 1'000/*total_vested_hive*/, 500'000/*total_vests*/, 0/*hive_modifier*/, 500'000/*vest_modifier*/ );
  _calculate_modifiers( 1'000/*price_hive_base*/, 1/*price_vests_quote*/, 500'000/*total_vested_hive*/, 1'000/*total_vests*/, 500'000/*hive_modifier*/, 0/*vest_modifier*/ );

  _calculate_modifiers( 2/*price_hive_base*/, 400/*price_vests_quote*/, 1'000/*total_vested_hive*/, 1'000'000/*total_vests*/, 4'000/*hive_modifier*/, 0/*vest_modifier*/ );

  _calculate_modifiers( 1/*price_hive_base*/, 500/*price_vests_quote*/, 800'000/*total_vested_hive*/, 100'000'000/*total_vests*/, 0/*hive_modifier*/, 300'000'000/*vest_modifier*/ );

  _calculate_modifiers( 1/*price_hive_base*/, 3/*price_vests_quote*/, 1'000/*total_vested_hive*/, 5'000/*total_vests*/, 666/*hive_modifier*/, 0/*vest_modifier*/ );
}

BOOST_AUTO_TEST_CASE( state_modification )
{
  auto _check_state = [ this ]( share_type price_hive_base, share_type price_vests_quote )
  {
    generate_block();
    validate_database();

    auto& _dgpo = db->get_dynamic_global_properties();

    char _buffer[500];

    sprintf( _buffer, "dgpo.total_vesting_fund_hive (%ld) dgpo.total_vesting_shares (%ld)", _dgpo.total_vesting_fund_hive.amount.value, _dgpo.total_vesting_shares.amount.value );
    BOOST_TEST_MESSAGE( _buffer );

    hive::protocol::HIVE_asset _old_current_supply = _dgpo.current_supply;
    hive::protocol::HIVE_asset _old_virtual_supply = _dgpo.virtual_supply;

    hive::protocol::HIVE_asset _old_total_vested_hive  = _dgpo.total_vesting_fund_hive;
    hive::protocol::VEST_asset _old_total_vests = _dgpo.total_vesting_shares;
    hive::protocol::HIVE_asset _hive_modifier;
    hive::protocol::VEST_asset _vest_modifier;
    hive::protocol::price _new_price = hive::protocol::price( hive::protocol::asset( price_hive_base, HIVE_SYMBOL ), hive::protocol::asset( price_vests_quote, VESTS_SYMBOL ) );

    db_plugin->calculate_modifiers_according_to_new_price( _new_price, _old_total_vested_hive, _old_total_vests, _hive_modifier, _vest_modifier );

    db_plugin->debug_set_vest_price( _new_price );

    sprintf( _buffer, "dgpo.current_supply (%ld) == _old_current_supply (%ld) + _hive_modifier (%ld)", _dgpo.current_supply.amount.value, _old_current_supply.amount.value, _hive_modifier.amount.value );
    BOOST_TEST_MESSAGE( _buffer );

    sprintf( _buffer, "dgpo.virtual_supply (%ld) == _old_virtual_supply (%ld) + _hive_modifier (%ld)", _dgpo.virtual_supply.amount.value, _old_virtual_supply.amount.value, _hive_modifier.amount.value );
    BOOST_TEST_MESSAGE( _buffer );

    sprintf( _buffer, "dgpo.total_vesting_fund_hive (%ld) == _old_total_vested_hive (%ld) + _hive_modifier (%ld)", _dgpo.total_vesting_fund_hive.amount.value, _old_total_vested_hive.amount.value, _hive_modifier.amount.value );
    BOOST_TEST_MESSAGE( _buffer );

    sprintf( _buffer, "dgpo.total_vesting_shares (%ld) == _old_total_vests (%ld) + _vest_modifier (%ld)", _dgpo.total_vesting_shares.amount.value, _old_total_vests.amount.value, _vest_modifier.amount.value );
    BOOST_TEST_MESSAGE( _buffer );

    BOOST_CHECK( _dgpo.current_supply          == _old_current_supply + _hive_modifier );
    BOOST_CHECK( _dgpo.virtual_supply          == _old_virtual_supply + _hive_modifier );

    BOOST_CHECK( _dgpo.total_vesting_fund_hive == _old_total_vested_hive + _hive_modifier );
    BOOST_CHECK( _dgpo.total_vesting_shares    == _old_total_vests + _vest_modifier );

    generate_block();
    validate_database();
  };

  {
    BOOST_TEST_MESSAGE("hive_modifier == 0 and vest_modifier > 0");
    _check_state( 1/*price_hive_base*/, 2'000'000'000/*price_vests_quote*/ );
  }

  {
    BOOST_TEST_MESSAGE("hive_modifier > 0 and vest_modifier == 0");
    _check_state( 1/*price_hive_base*/, 10'000/*price_vests_quote*/ );
  }

}

BOOST_AUTO_TEST_CASE( debug_update_use_bug )
{
  BOOST_TEST_MESSAGE( "Illustration for bug in debug_set_vest_price" );

  db_plugin->debug_update( []( database& db )
  {
    BOOST_TEST_MESSAGE( "First set some value in state so we can check if it was changed later" );
    db.modify( db.get_dynamic_global_properties(), [&]( dynamic_global_property_object& gpo )
    {
      gpo.maximum_block_size = HIVE_MIN_BLOCK_SIZE_LIMIT;
    } );
  } );

  generate_block();

  db_plugin->debug_set_vest_price( hive::protocol::price( hive::protocol::asset( 1, HIVE_SYMBOL ), hive::protocol::asset( 2000, VESTS_SYMBOL ) ) );

  // when your call to debug_update refers to local variables you must make sure they will remain
  // alive at least until registered action can no longer be called again
  uint32_t BAADF00D = 0xBAADF00D;
  uint32_t* stack_cleaner = ( uint32_t* )alloca( sizeof( BAADF00D ) * 100 );
  std::fill_n( stack_cleaner, 100, BAADF00D );

  BOOST_REQUIRE_EQUAL( db->get_dynamic_global_properties().maximum_block_size, HIVE_MIN_BLOCK_SIZE_LIMIT );
  const uint32_t new_value = HIVE_MIN_BLOCK_SIZE_LIMIT * 4;

  // in order to show the bug it is important that this debug_update is called in the same block as
  // previous one hidden inside debug_set_vest_price()
  db_plugin->debug_update( [new_value]( database& db )
  {
    BOOST_TEST_MESSAGE( "Second call to debug_update within the same block reveals problem" );
    db.modify( db.get_dynamic_global_properties(), [&]( dynamic_global_property_object& gpo )
    {
      gpo.maximum_block_size = new_value;
    } );
  } );

  BOOST_TEST_MESSAGE( "Make sure the value we've set in second debug_update was actually changed" );
  BOOST_REQUIRE_EQUAL( db->get_dynamic_global_properties().maximum_block_size, new_value );

  generate_block();
  BOOST_TEST_MESSAGE( "The value should still be set after generating block" );
    // if we generated more blocks the value would eventually be overwritten from witness values
  BOOST_REQUIRE_EQUAL( db->get_dynamic_global_properties().maximum_block_size, new_value );

  validate_database();
}

BOOST_AUTO_TEST_SUITE_END()
#endif
