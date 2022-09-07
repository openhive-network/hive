#pragma once
#include <hive/protocol/base.hpp>
#include <hive/protocol/operation_util.hpp>

#include <hive/chain/evaluator.hpp>


namespace hive { namespace plugins { namespace follow {

using namespace std;
using hive::protocol::account_name_type;
using hive::protocol::base_operation;

class follow_plugin;

struct follow_operation : base_operation
{
  account_name_type follower;
  account_name_type following;
  set< string >     what; /// blog, mute

  void validate()const;

  void get_required_posting_authorities( flat_set<account_name_type>& a )const { a.insert( follower ); }
};

struct reblog_operation : base_operation
{
  account_name_type account;
  account_name_type author;
  string            permlink;

  void validate()const;

  void get_required_posting_authorities( flat_set<account_name_type>& a )const { a.insert( account ); }
};

typedef fc::static_variant<
    follow_operation,
    reblog_operation
  > follow_plugin_operation;

HIVE_DEFINE_PLUGIN_EVALUATOR( follow_plugin, follow_plugin_operation, follow );
HIVE_DEFINE_PLUGIN_EVALUATOR( follow_plugin, follow_plugin_operation, reblog );

} } } // hive::plugins::follow

namespace fc
{
  using hive::plugins::follow::follow_plugin_operation;
  template<>
  struct serialization_functor< follow_plugin_operation >
  {
    bool operator()( const fc::variant& v, follow_plugin_operation& s ) const
    {
      return extended_serialization_functor< follow_plugin_operation >().serialize( v, s );
    }
  };

  template<>
  struct variant_creator_functor< follow_plugin_operation >
  {
    template<typename T>
    fc::variant operator()( const T& v ) const
    {
      return extended_variant_creator_functor< follow_plugin_operation >().create( v );
    }
  };
}

FC_REFLECT( hive::plugins::follow::follow_operation, (follower)(following)(what) )
FC_REFLECT( hive::plugins::follow::reblog_operation, (account)(author)(permlink) )

HIVE_DECLARE_OPERATION_TYPE( hive::plugins::follow::follow_plugin_operation )

FC_REFLECT_TYPENAME( hive::plugins::follow::follow_plugin_operation )
