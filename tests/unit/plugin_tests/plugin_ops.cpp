#ifdef IS_TEST_NET
#include <hive/chain/account_object.hpp>
#include <hive/chain/database.hpp>
#include <hive/chain/generic_custom_operation_interpreter.hpp>

#include <boost/test/unit_test.hpp>

#include "../db_fixture/database_fixture.hpp"

using namespace hive::chain;
using namespace hive::chain::test;
/*
namespace hive { namespace plugin_tests {

using namespace hive::app;
using namespace hive::chain;

struct test_a_operation : base_operation
{
  string account;

  void validate() { FC_ASSERT( account.size() ); }
};

struct test_b_operation : base_operation
{
  string account;

  void validate() { FC_ASSERT( account.size() ); }
};

typedef fc::static_variant<
  test_a_operation,
  test_b_operation >   test_op;

class test_plugin : public plugin
{
  public:
    test_plugin( application* app );

    std::string plugin_name()const override { return "TEST"; }

    std::shared_ptr< generic_custom_operation_interpreter< test_op > > _evaluator_registry;
};

HIVE_DEFINE_PLUGIN_EVALUATOR( test_plugin, test_a_operation, test_a );
HIVE_DEFINE_PLUGIN_EVALUATOR( test_plugin, test_b_operation, test_b );

void test_a_evaluator::do_apply( const test_a_operation& o )
{
  const auto& account = db().get_account( o.account );

  db().modify( account, [&]( account_object& a )
  {
    a.json_metadata = "a";
  });
}

void test_b_evaluator::do_apply( const test_b_operation& o )
{
  const auto& account = db().get_account( o.account );

  db().modify( account, [&]( account_object& a )
  {
    a.json_metadata = "b";
  });
}

test_plugin::test_plugin( application* app ) : plugin( app )
{
  _evaluator_registry = std::make_shared< generic_custom_operation_interpreter< test_op > >( database() );

  _evaluator_registry->register_evaluator< test_a_evaluator >( *this );
  _evaluator_registry->register_evaluator< test_b_evaluator >( *this );

  database().set_custom_operation_interpreter( plugin_name(), _evaluator_registry );
}

} } // hive::plugin_tests

HIVE_DEFINE_PLUGIN( test, hive::plugin_tests::test_plugin )

FC_REFLECT( hive::plugin_tests::test_a_operation, (account) )
FC_REFLECT( hive::plugin_tests::test_b_operation, (account) )

HIVE_DECLARE_OPERATION_TYPE( hive::plugin_tests::test_op );
FC_REFLECT_TYPENAME( hive::plugin_tests::test_op );
HIVE_DEFINE_OPERATION_TYPE( hive::plugin_tests::test_op );
*/


#endif