#pragma once
#include <hive/chain/hive_fwd.hpp>
#include <hive/chain/util/balance.hpp>
#include <hive/chain/hive_object_types.hpp>

namespace hive { namespace chain {

using hive::protocol::HBD_asset;
class account_object;

/**
  *  This object is used to track pending requests to convert HBD to HIVE
  */
class convert_request_object : public object< convert_request_object_type, convert_request_object >
{
  CHAINBASE_OBJECT( convert_request_object );
public:
  template< typename Allocator >
  convert_request_object( allocator< Allocator > a, uint64_t _id,
    const account_object& _owner, temp_HBD_balance&& _amount, const time_point_sec& _conversion_time, uint32_t _requestid )
  : id( _id ), amount( std::move( _amount ) ), requestid( _requestid ), conversion_date( _conversion_time )
  {
    init( _owner );
  }

// getters:

  //amount of HBD to be converted to HIVE
  const HBD_asset& get_convert_amount() const { return amount; }

  //request owner
  account_id_type get_owner() const { return owner; }
  //request id - unique id of request among active requests of the same owner
  uint32_t get_request_id() const { return requestid; }
  //time of actual conversion
  time_point_sec get_conversion_date() const { return conversion_date; }

  void check_on_remove() const
  {
    FC_ASSERT( amount.is_empty(), "Removing convert_request_object with non-empty balance field" );
  }

// setters:

  HBD_balance& access_amount() { return amount; }

private:
  account_id_type owner;
  HBD_balance     amount;
  uint32_t        requestid = 0; //< id set by owner, the owner,requestid pair must be unique
  time_point_sec  conversion_date; //< actual conversion takes place at that time

  void init( const account_object& _owner );

  CHAINBASE_UNPACK_CONSTRUCTOR( convert_request_object );
};

} } // hive::chain

FC_REFLECT( hive::chain::convert_request_object,
          (id)(owner)(amount)(requestid)(conversion_date) )
