#if defined IS_TEST_NET

#include "condenser_api_reversible_fixture.hpp"

#include <hive/plugins/database_api/database_api.hpp>

#include <boost/test/unit_test.hpp>

condenser_api_reversible_fixture::condenser_api_reversible_fixture()
{
    db->set_hardfork( HIVE_NUM_HARDFORKS );
    generate_block();

    ACTORS((top1)(top2)(top3)(top4)(top5)(top6)(top7)(top8)(top9)(top10)
           (top11)(top12)(top13)(top14)(top15)(top16)(top17)(top18)(top19)(top20))
    ACTORS((backup1)(backup2)(backup3)(backup4)(backup5)(backup6)(backup7)(backup8)(backup9)(backup10))

    const auto create_witness = [&]( const std::string& witness_name )
    {
      const private_key_type account_key = generate_private_key( witness_name );
      const private_key_type witness_key = generate_private_key( witness_name + "_witness" );
      witness_create( witness_name, account_key, witness_name + ".com", witness_key.get_public_key(), 1000 );
    };
    for( int i = 1; i <= 20; ++i )
      create_witness( "top" + std::to_string(i) );
    for( int i = 1; i <= 10; ++i )
      create_witness( "backup" + std::to_string(i) );

    ACTORS((whale)(voter1)(voter2)(voter3)(voter4)(voter5)(voter6)(voter7)(voter8)(voter9)(voter10))

    fund( "whale", 500000000 );
    vest( "whale", 500000000 );

    account_witness_vote_operation op;
    op.account = "whale";
    op.approve = true;
    for( int v = 1; v <= 20; ++v )
    {
      op.witness = "top" + std::to_string(v);
      push_transaction( op, whale_private_key );
    }

    for( int i = 1; i <= 10; ++i )
    {
      std::string name = "voter" + std::to_string(i);
      fund( name, 10000000 );
      vest( name, 10000000 / i );
      op.account = name;
      for( int v = 1; v <= i; ++v )
      {
        op.witness = "backup" + std::to_string(v);
        push_transaction( op, generate_private_key(name) );
      }
    }

    generate_block();

    generate_until_block(43);

    BOOST_REQUIRE_EQUAL( database_api->get_active_witnesses({}).witnesses.size(), 21 );
}

condenser_api_reversible_fixture::~condenser_api_reversible_fixture()
{
  // do nothing
}

#endif
