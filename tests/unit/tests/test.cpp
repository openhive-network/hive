
#include <boost/test/unit_test.hpp>

#include <hive/chain/hive_fwd.hpp>

#include <hive/protocol/exceptions.hpp>
#include <hive/protocol/hardfork.hpp>
#include <hive/protocol/sps_operations.hpp>

#include <hive/chain/database.hpp>
#include <hive/chain/database_exceptions.hpp>
#include <hive/chain/hive_objects.hpp>

#include <hive/chain/util/reward.hpp>
#include <hive/chain/util/hf23_helper.hpp>

#include <hive/plugins/rc/rc_objects.hpp>
#include <hive/plugins/rc/resource_count.hpp>

#include <fc/macros.hpp>
#include <fc/crypto/digest.hpp>

#include "../db_fixture/database_fixture.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace hive;
using namespace hive::chain;
using namespace hive::protocol;
using fc::string;


BOOST_FIXTURE_TEST_SUITE( tests, clean_database_fixture )

BOOST_AUTO_TEST_CASE( is_transfer_working )
{
  try
  {
    auto get_value = [](const asset& v) -> int64_t { return v.amount.value; };

    // setup
    ACTORS( (alice)(bob) )
    generate_block();
    transfer( HIVE_INIT_MINER_NAME, "alice", asset( 1000000, HIVE_SYMBOL ) );

    const auto alice_init_balance = get_balance("alice");
    const auto bob_init_balance = get_balance("bob");

    // actions
    generate_blocks(15);
    const uint64_t value_to_transfer{ 100000ul };
    transfer("alice", "bob", asset(value_to_transfer, HIVE_SYMBOL));
    generate_blocks(15);

    // after checks
    const auto alice_after_balance = get_balance("alice");
    const auto bob_after_balance = get_balance("bob");

    BOOST_REQUIRE_EQUAL(get_value(alice_after_balance), get_value(alice_init_balance) - value_to_transfer);
    BOOST_REQUIRE_EQUAL(get_value(bob_after_balance), get_value(bob_init_balance) + value_to_transfer);
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()
