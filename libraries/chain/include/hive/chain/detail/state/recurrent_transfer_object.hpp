#pragma once
#include <hive/chain/hive_fwd.hpp>
#include <hive/protocol/asset.hpp>
#include <hive/chain/hive_object_types.hpp>
#include <hive/chain/detail/state/account_object.hpp>

namespace hive { namespace chain {

using hive::protocol::asset;

class recurrent_transfer_object : public object< recurrent_transfer_object_type, recurrent_transfer_object, std::true_type >
{
  CHAINBASE_OBJECT( recurrent_transfer_object );
public:
  template< typename Allocator >
  recurrent_transfer_object( allocator< Allocator > a, uint64_t _id,
    const time_point_sec& _trigger_date, const account_object& _from,
    const account_object& _to, const asset& _amount, const string& _memo,
    const uint16_t _recurrence, const uint16_t _remaining_executions, const uint8_t _pair_id )
  : id( _id ), trigger_date( _trigger_date ), from_id( _from.get_id() ), to_id( _to.get_id() ),
    amount( _amount ), memo( a ), recurrence( _recurrence ), remaining_executions( _remaining_executions ), pair_id( _pair_id )
  {
    from_string( memo, _memo );
  }

  void update_next_trigger_date()
  {
    trigger_date += fc::hours(recurrence);
  }

  time_point_sec get_trigger_date() const
  {
    return trigger_date;
  }

  time_point_sec get_final_trigger_date() const
  {
    return trigger_date + fc::hours( recurrence * ( remaining_executions - 1 ) );
  }

  // if the recurrence changed, we must update the trigger_date
  void set_recurrence_trigger_date( const time_point_sec& _head_block_time, uint16_t _recurrence )
  {
    if ( _recurrence != recurrence )
      trigger_date = _head_block_time + fc::hours( _recurrence );

    recurrence = _recurrence;
  }

private:
  time_point_sec    trigger_date;
public:
  account_id_type   from_id;
  account_id_type   to_id;
  asset             amount;
  /// The memo is plain-text, any encryption on the memo is up to a higher level protocol.
  shared_string     memo;
  /// How often will the payment be triggered, unit: hours
  uint16_t          recurrence = 0;
  /// How many executions are remaining
  uint16_t          remaining_executions = 0;
  /// How many payment have failed in a row, at HIVE_MAX_CONSECUTIVE_RECURRENT_TRANSFER_FAILURES the object is deleted
  uint8_t           consecutive_failures = 0;
  uint8_t           pair_id = 0;

  size_t get_dynamic_alloc() const
  {
    size_t size = 0;
    size += memo.capacity() * sizeof( decltype( memo )::value_type );
    return size;
  }

  CHAINBASE_UNPACK_CONSTRUCTOR( recurrent_transfer_object, (memo) );
};

} } // hive::chain

FC_REFLECT(hive::chain::recurrent_transfer_object, (id)(trigger_date)(from_id)(to_id)(amount)(memo)(recurrence)(remaining_executions)(consecutive_failures)(pair_id) )
