#pragma once
#include <hive/chain/hive_fwd.hpp>
#include <hive/chain/util/balance.hpp>
#include <hive/chain/hive_object_types.hpp>

namespace hive { namespace chain {

using hive::protocol::asset;
class account_object;

class savings_withdraw_object : public object< savings_withdraw_object_type, savings_withdraw_object, std::true_type >
{
  CHAINBASE_OBJECT( savings_withdraw_object );

public:
  template< typename Allocator >
  savings_withdraw_object( allocator< Allocator > a, uint64_t _id,
    const account_object& _from, const account_object& _to, temp_balance&& _amount,
    const string& _memo, const time_point_sec& _time_of_completion, uint32_t _request_id )
  : id( _id ), memo( a ), request_id( _request_id ),
    amount( std::move( _amount ) ), complete( _time_of_completion )
  {
    init( _from, _to, _memo );
  }

// getters:

  //amount of savings to withdraw (HIVE or HBD)
  const asset& get_withdraw_amount() const { return amount; }

  const account_name_type& get_from() const { return from; }
  const account_name_type& get_to() const { return to; }
  const shared_string& get_memo() const { return memo; }
  uint32_t get_request_id() const { return request_id; }
  time_point_sec get_completion_time() const { return complete; }

  void check_on_remove() const
  {
    FC_ASSERT( amount.is_empty(), "Removing savings_withdraw_object with non-empty balance field" );
  }

  size_t get_dynamic_alloc() const
  {
    size_t size = 0;
    size += memo.capacity() * sizeof( decltype( memo )::value_type );
    return size;
  }

// setters:

  balance& access_amount() { return amount; }

private:
  account_name_type from; // can't be replaced with account_id_type - see database_api.list_savings_withdrawals API
  account_name_type to; // can't be replaced with account_id_type - see database_api.list_savings_withdrawals API
  shared_string     memo;
  uint32_t          request_id = 0;
  balance           amount; // can hold HIVE or HBD
  time_point_sec    complete;

  void init( const account_object& _from, const account_object& _to, const string& _memo );

  CHAINBASE_UNPACK_CONSTRUCTOR( savings_withdraw_object, (memo) );
};

} } // hive::chain

FC_REFLECT( hive::chain::savings_withdraw_object,
          (id)(from)(to)(memo)(request_id)(amount)(complete) )
