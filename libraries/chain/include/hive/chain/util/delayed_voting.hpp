#pragma once

#include <chainbase/allocators.hpp>

#include <hive/chain/account_object.hpp>
#include <hive/chain/database.hpp>
#include <hive/chain/index.hpp>

namespace hive { namespace chain {

struct votes_update_data
{
  bool                    withdraw_executor = false;
  mutable share_type      val = 0;

  const account_object*   account = nullptr;
};

struct votes_update_data_less
{
  bool operator()( const votes_update_data& obj1, const votes_update_data& obj2 ) const 
  {
    FC_ASSERT( obj1.account && obj2.account, "unexpected error: ${error}", ("error", delayed_voting_messages::object_is_null ) );
    return obj1.account->get_id() < obj2.account->get_id();
  }
};

class delayed_voting
{
  public:

    using votes_update_data_items = std::set< votes_update_data, votes_update_data_less >;
    using opt_votes_update_data_items = fc::optional< votes_update_data_items >;

  private:

    chain::database& db;

    void erase_delayed_value( const account_object& account, const ushare_type val );

  public:

    delayed_voting( chain::database& _db ) : db( _db ){}

    void add_delayed_value( const account_object& account, const time_point_sec& head_time, const ushare_type val );
    void add_votes( opt_votes_update_data_items& items, const bool withdraw_executor, const share_type& val, const account_object& account );
    fc::optional< ushare_type > update_votes( const opt_votes_update_data_items& items, const time_point_sec& head_time );

    void run( const fc::time_point_sec& head_time );
};

} } // namespace hive::chain
