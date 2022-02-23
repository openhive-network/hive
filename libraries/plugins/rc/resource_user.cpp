
#include <hive/plugins/rc/resource_user.hpp>

#include <hive/protocol/transaction.hpp>

namespace hive { namespace plugins { namespace rc {

using namespace hive::protocol;

struct get_resource_user_visitor
{
  typedef account_name_type result_type;

  get_resource_user_visitor() {}

  account_name_type operator()( const witness_set_properties_operation& op )const
  {
    return op.owner;
  }

  account_name_type operator()( const recover_account_operation& op )const
  {
    return op.account_to_recover;
  }

  template< typename Op >
  account_name_type operator()( const Op& op )const
  {
    flat_set< account_name_type > req;
    op.get_required_active_authorities( req );
    for( const account_name_type& account : req )
      return account;
    op.get_required_owner_authorities( req );
    for( const account_name_type& account : req )
      return account;
    op.get_required_posting_authorities( req );
    for( const account_name_type& account : req )
      return account;
    return account_name_type();
  }
};

account_name_type get_resource_user( const signed_transaction& tx )
{
  get_resource_user_visitor vtor;

  for( const operation& op : tx.operations )
  {
    account_name_type resource_user = op.visit( vtor );
    if( resource_user != account_name_type() )
      return resource_user;
  }
  return account_name_type();
}

account_name_type get_resource_user( const optional_automated_action& action )
{
  get_resource_user_visitor vtor;

  return action.visit( vtor );
}

} } } // hive::plugins::rc
