#ifdef IS_TEST_NET

#include <boost/test/unit_test.hpp>

#include <hive/chain/detail/state/witness_objects.hpp>

#include <hive/protocol/forward_keyauths.hpp>

#include "../db_fixture/clean_database_fixture.hpp"

using namespace hive::chain;
using namespace hive::protocol;

namespace
{

// keyauth keeps the current WITNESS_SIGNING key of each witness, so the key collected from a witness operation
// must be the one the witness signs blocks with after that operation.
void require_collected_signing_key( const operation& op, const witness_object& witness )
{
  std::vector< public_key_type > keys;
  for( const auto& item : hive::app::operation_get_keyauths( op ) )
  {
    if( item.key_kind == hive::app::key_t::WITNESS_SIGNING && item.keyauth_variant )
    {
      BOOST_REQUIRE_EQUAL( std::string( item.account_name ), std::string( witness.owner ) );
      keys.emplace_back( item.key_auth );
    }
  }
  BOOST_REQUIRE_EQUAL( keys.size(), 1u );
  BOOST_REQUIRE_EQUAL( std::string( keys.front() ), std::string( witness.signing_key ) );
}

} // namespace

BOOST_FIXTURE_TEST_SUITE( keyauth_tests, clean_database_fixture )

BOOST_AUTO_TEST_CASE( witness_signing_key_follows_signing_key_changes )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: WITNESS_SIGNING keyauth of witness_update and witness_set_properties" );

    ACTORS( HIVE_asset( 15'000 ), (alice) );
    fund( "alice", HIVE_asset( 10'000 ) );
    private_key_type old_signing_key = generate_private_key( "old_key" );
    private_key_type new_signing_key = generate_private_key( "new_key" );

    witness_update_operation update_op;
    update_op.owner = "alice";
    update_op.url = "foo.bar";
    update_op.fee = ASSET( "1.000 TESTS" );
    update_op.block_signing_key = old_signing_key.get_public_key();
    push_transaction( update_op, alice_private_key );

    const witness_object& alice_witness = db->get_witness( "alice" );
    BOOST_REQUIRE( alice_witness.signing_key == old_signing_key.get_public_key() );
    require_collected_signing_key( update_op, alice_witness );

    BOOST_TEST_MESSAGE( "--- Signing key unchanged: the collected key is 'key'" );
    witness_set_properties_operation prop_op;
    prop_op.owner = "alice";
    prop_op.props[ "key" ] = fc::raw::pack_to_vector( old_signing_key.get_public_key() );
    prop_op.props[ "url" ] = fc::raw::pack_to_vector( std::string( "foo.bar.baz" ) );
    push_transaction( prop_op, old_signing_key );
    BOOST_REQUIRE( alice_witness.signing_key == old_signing_key.get_public_key() );
    require_collected_signing_key( prop_op, alice_witness );

    BOOST_TEST_MESSAGE( "--- Signing key changed: the collected key is 'new_signing_key', not 'key'" );
    prop_op.props.erase( "url" );
    prop_op.props[ "new_signing_key" ] = fc::raw::pack_to_vector( new_signing_key.get_public_key() );
    push_transaction( prop_op, old_signing_key );
    BOOST_REQUIRE( alice_witness.signing_key == new_signing_key.get_public_key() );
    require_collected_signing_key( prop_op, alice_witness );

    BOOST_TEST_MESSAGE( "--- Witness disabled: the collected key is the null key" );
    prop_op.props[ "key" ] = fc::raw::pack_to_vector( new_signing_key.get_public_key() );
    prop_op.props[ "new_signing_key" ] = fc::raw::pack_to_vector( public_key_type() );
    push_transaction( prop_op, new_signing_key );
    BOOST_REQUIRE( alice_witness.signing_key == public_key_type() );
    require_collected_signing_key( prop_op, alice_witness );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()

#endif
