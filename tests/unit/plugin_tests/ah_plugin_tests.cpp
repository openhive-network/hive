#ifdef IS_TEST_NET
#include <boost/test/unit_test.hpp>

#include "../db_fixture/database_fixture.hpp"

using namespace hive::chain;
using namespace hive::protocol;
using namespace hive::plugins;
using namespace hive::plugins::account_history_rocksdb;

BOOST_FIXTURE_TEST_SUITE( ah_plugin_tests, clean_database_fixture )

BOOST_AUTO_TEST_CASE( get_ops_in_block_zero_bug )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing bug in get_ops_in_block with block 0" );

    //instead of using api (which we don't have here and it's a pain to install), we are using AH plugin
    //directly - routine below is used by account_history_api::get_ops_in_block
    ah_plugin->find_operations_by_block( 0, true, []( const rocksdb_operation_object& op )
    {
      BOOST_REQUIRE( false && "List of operations should be empty" );
    } );
    //the bug was with block 0 that was waiting on lock until irreversible state changed, which could
    //take some time (and since unit tests work on single thread this particular test was waiting
    //indefinitely - same happened with stopped node, only live node would give response, but after long
    //time)
    
    BOOST_TEST_MESSAGE( "If you are here it means the test works" );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( enum_virtual_ops_zero_bug )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing bug in enum_virtual_ops with block 0" );

    //the cause and fix is the same as in get_ops_in_block_zero_bug
    ah_plugin->enum_operations_from_block_range( 0, 9, true, 0, 5,
      []( const rocksdb_operation_object&, uint64_t, bool ) { return true; } );

    BOOST_TEST_MESSAGE( "If you are here it means the test works" );
  }
  FC_LOG_AND_RETHROW()
}

struct trigger_bug : appbase::plugin< trigger_bug >
{
  database& _db;
  boost::signals2::connection _post_apply_block;
  bool trigger = false;

  trigger_bug( database& db ) : _db( db )
  {
    _post_apply_block = _db.add_post_apply_block_handler( [this]( const block_notification& )
    {
      if( trigger )
      {
        trigger = false;
        FC_ASSERT( false ); //trigger problem in plugin
      }
    }, *this, -100 );
  }
  virtual ~trigger_bug()
  {
    chain::util::disconnect_signal( _post_apply_block );
  }

  static const std::string& name() { static std::string name = "bug"; return name; }
private: //just because it is (almost unused) part of signal registration
  virtual void set_program_options( appbase::options_description& cli, appbase::options_description& cfg ) override {}
  virtual void plugin_for_each_dependency( plugin_processor&& processor ) override {}
  virtual void plugin_initialize( const appbase::variables_map& options ) override {}
  virtual void plugin_startup() override {}
  virtual void plugin_shutdown() override {}
};

BOOST_AUTO_TEST_CASE( mutex_reentry_bug )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing bug where mutex used to stop API during block processing is not unlocked" );

    trigger_bug bug( *db );

    bug.trigger = true;
    generate_block();
    generate_block(); //second block caused reentry on _currently_processed_block_mtx mutex and
      //deadlock; we could achieve the same effect, although only in unit test, by using AH API
      //asking for reversible data; BTW. "buggy plugin" only works as bug trigger because plugin
      //signals are not isolated yet (see issue#255)

    BOOST_TEST_MESSAGE( "If you are here it means the test works" );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()

#endif
