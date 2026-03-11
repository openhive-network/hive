#pragma once
#include <hive/chain/hive_fwd.hpp>
#include <hive/protocol/asset.hpp>
#include <hive/chain/hive_object_types.hpp>

namespace hive { namespace chain {

  class account_object;

  using hive::protocol::HIVE_asset;
  using hive::protocol::HBD_asset;
  using hive::protocol::asset;

  class escrow_object : public object< escrow_object_type, escrow_object >
  {
    CHAINBASE_OBJECT( escrow_object );
    public:
      template< typename Allocator >
      escrow_object( allocator< Allocator > a, uint64_t _id,
        const account_object& _from, const account_object& _to, const account_object& _agent,
        const HIVE_asset& _hive_amount, const HBD_asset& _hbd_amount, const asset& _fee,
        const time_point_sec& _ratification_deadline, const time_point_sec& _escrow_expiration, uint32_t _escrow_transfer_id )
      : id( _id ), escrow_id( _escrow_transfer_id ),
        ratification_deadline( _ratification_deadline ), escrow_expiration( _escrow_expiration ),
        hbd_balance( _hbd_amount ), hive_balance( _hive_amount ), pending_fee( _fee )
      {
        init( _from, _to, _agent );
      }

    // getters:

      uint32_t get_escrow_id() const { return escrow_id; }
      const account_name_type& get_from() const { return from; }
      const account_name_type& get_to() const { return to; }
      const account_name_type& get_agent() const { return agent; }
      time_point_sec get_ratification_deadline() const { return ratification_deadline; }
      time_point_sec get_escrow_expiration() const { return escrow_expiration; }

      //HIVE portion of transfer balance
      const HIVE_asset& get_hive_balance() const { return hive_balance; }
      //HBD portion of transfer balance
      const HBD_asset& get_hbd_balance() const { return hbd_balance; }
      //fee offered to escrow (can be either in HIVE or HBD)
      const asset& get_fee() const { return pending_fee; }

      bool is_to_approved() const { return to_approved; }
      bool is_agent_approved() const { return agent_approved; }
      bool is_approved() const { return to_approved && agent_approved; }
      bool is_disputed() const { return disputed; }

   // setters:

      HIVE_asset& access_hive_balance() { return hive_balance; }
      HBD_asset& access_hbd_balance() { return hbd_balance; }
      asset& access_fee() { return pending_fee; }

      void approve_to() { to_approved = true; }
      void approve_agent() { agent_approved = true; }
      void start_dispute() { disputed = true; }

    private:
      uint32_t          escrow_id = 20;
      account_name_type from; //< TODO: can be replaced with account_id_type (ABW: would make looking for escrow object harder)
      account_name_type to; //< TODO: can be replaced with account_id_type (ABW: would make looking for escrow object harder)
      account_name_type agent; //< TODO: can be replaced with account_id_type (ABW: would make looking for escrow object harder)
      time_point_sec    ratification_deadline;
      time_point_sec    escrow_expiration;
      HBD_asset         hbd_balance;
      HIVE_asset        hive_balance;
      asset             pending_fee; //fee can use HIVE of HBD
      bool              to_approved = false; //< TODO: can be replaced with bit field along with all flags
      bool              agent_approved = false;
      bool              disputed = false;

      void init( const account_object& _from, const account_object& _to, const account_object& _agent );

    CHAINBASE_UNPACK_CONSTRUCTOR( escrow_object );
  };

} } // hive::chain

FC_REFLECT( hive::chain::escrow_object,
          (id)(escrow_id)(from)(to)(agent)
          (ratification_deadline)(escrow_expiration)
          (hbd_balance)(hive_balance)(pending_fee)
          (to_approved)(agent_approved)(disputed) )
