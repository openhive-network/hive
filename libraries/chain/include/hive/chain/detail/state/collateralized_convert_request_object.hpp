#pragma once
#include <hive/chain/hive_fwd.hpp>
#include <hive/protocol/asset.hpp>
#include <hive/chain/hive_object_types.hpp>
#include <hive/chain/account_object.hpp>

namespace hive { namespace chain {

  using hive::protocol::HBD_asset;
  using hive::protocol::HIVE_asset;

  /**
    *  This object is used to track pending requests to convert HIVE to HBD with collateral
    */
  class collateralized_convert_request_object : public object< collateralized_convert_request_object_type, collateralized_convert_request_object >
  {
    CHAINBASE_OBJECT( collateralized_convert_request_object );
    public:
      template< typename Allocator >
      collateralized_convert_request_object( allocator< Allocator > a, uint64_t _id,
        const account_object& _owner, const HIVE_asset& _collateral_amount, const HBD_asset& _converted_amount,
        const time_point_sec& _conversion_time, uint32_t _requestid )
        : id( _id ), owner( _owner.get_id() ), collateral_amount( _collateral_amount ), converted_amount( _converted_amount ),
        requestid( _requestid ), conversion_date( _conversion_time )
      {}

      //amount of HIVE collateral
      const HIVE_asset& get_collateral_amount() const { return collateral_amount; }
      //amount of HBD that was already given to owner
      const HBD_asset& get_converted_amount() const { return converted_amount; }

      //request owner
      account_id_type get_owner() const { return owner; }
      //request id - unique id of request among active requests of the same owner
      uint32_t get_request_id() const { return requestid; }
      //time of actual conversion
      time_point_sec get_conversion_date() const { return conversion_date; }

    private:
      account_id_type owner;
      HIVE_asset      collateral_amount;
      HBD_asset       converted_amount;
      uint32_t        requestid = 0; //< id set by owner, the (owner,requestid) pair must be unique
      time_point_sec  conversion_date; //< actual conversion takes place at that time, excess collateral is returned to owner

      CHAINBASE_UNPACK_CONSTRUCTOR( collateralized_convert_request_object );
  };

} } // hive::chain

FC_REFLECT( hive::chain::collateralized_convert_request_object,
          (id)(owner)(collateral_amount)(converted_amount)(requestid)(conversion_date) )
