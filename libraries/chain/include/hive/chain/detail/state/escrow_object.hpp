#pragma once
#include <hive/chain/hive_fwd.hpp>
#include <hive/protocol/asset.hpp>
#include <hive/chain/hive_object_types.hpp>

namespace hive { namespace chain {

  using hive::protocol::asset;

  class escrow_object : public object< escrow_object_type, escrow_object >
  {
    CHAINBASE_OBJECT( escrow_object );
    public:
      template< typename Allocator >
      escrow_object( allocator< Allocator > a, uint64_t _id,
        const account_name_type& _from, const account_name_type& _to, const account_name_type& _agent,
        const asset& _hive_amount, const asset& _hbd_amount, const asset& _fee,
        const time_point_sec& _ratification_deadline, const time_point_sec& _escrow_expiration, uint32_t _escrow_transfer_id )
        : id( _id ), escrow_id( _escrow_transfer_id ), from( _from ), to( _to ), agent( _agent ),
        ratification_deadline( _ratification_deadline ), escrow_expiration( _escrow_expiration ),
        hbd_balance( _hbd_amount ), hive_balance( _hive_amount ), pending_fee( _fee )
      {}

      //HIVE portion of transfer balance
      const asset& get_hive_balance() const { return hive_balance; }
      //HBD portion of transfer balance
      const asset& get_hbd_balance() const { return hbd_balance; }
      //fee offered to escrow (can be either in HIVE or HBD)
      const asset& get_fee() const { return pending_fee; }

      bool is_approved() const { return to_approved && agent_approved; }

      uint32_t          escrow_id = 20;
      account_name_type from; //< TODO: can be replaced with account_id_type
      account_name_type to; //< TODO: can be replaced with account_id_type
      account_name_type agent; //< TODO: can be replaced with account_id_type
      time_point_sec    ratification_deadline;
      time_point_sec    escrow_expiration;
      asset             hbd_balance; //< TODO: can be replaced with HBD_asset
      asset             hive_balance; //< TODO: can be replaced with HIVE_asset
      asset             pending_fee; //fee can use HIVE of HBD
      bool              to_approved = false; //< TODO: can be replaced with bit field along with all flags
      bool              agent_approved = false;
      bool              disputed = false;

    CHAINBASE_UNPACK_CONSTRUCTOR(escrow_object);
  };

} } // hive::chain

FC_REFLECT( hive::chain::escrow_object,
          (id)(escrow_id)(from)(to)(agent)
          (ratification_deadline)(escrow_expiration)
          (hbd_balance)(hive_balance)(pending_fee)
          (to_approved)(agent_approved)(disputed) )
