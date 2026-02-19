#pragma once

#include <hive/chain/hive_fwd.hpp>
#include <hive/chain/hive_object_types.hpp>
#include <hive/chain/util/manabar.hpp>

namespace hive { namespace chain {

  class manabars_rc_object : public object< manabars_rc_object_type, manabars_rc_object, std::false_type /* no dynamic alloc */ >
  {
    CHAINBASE_OBJECT( manabars_rc_object );
    public:
      template< typename Allocator >
      manabars_rc_object( allocator< Allocator > a, uint64_t _id,
        account_id_type _account_id,
        const time_point_sec& _creation_time = time_point_sec(),
        bool _fill_mana = false,
        int64_t _rc_adjustment = 0,
        share_type effective_vesting_shares = 0 )
        : id( _id ), account_id( _account_id ), rc_adjustment( _rc_adjustment )
      {
        voting_manabar.last_update_time = _creation_time.sec_since_epoch();
        downvote_manabar.last_update_time = _creation_time.sec_since_epoch();
        if( _fill_mana )
          voting_manabar.current_mana = HIVE_100_PERCENT;
        if( rc_adjustment.value )
        {
          rc_manabar.last_update_time = _creation_time.sec_since_epoch();
          auto max_rc = get_maximum_rc( effective_vesting_shares ).value;
          rc_manabar.current_mana = max_rc;
          last_max_rc = max_rc;
        }
      }

      // Link to parent account_object
      account_id_type get_account_id() const { return account_id; }

      // Effective balance of VESTS for RC calculation optionally excluding part that cannot be delegated
      share_type get_maximum_rc( share_type effective_vesting_shares, bool only_delegable = false ) const
      {
        share_type total = effective_vesting_shares - delegated_rc;
        if( only_delegable == false )
          total += rc_adjustment + received_rc;
        return total;
      }

      // RC compensation for account creation fee
      share_type get_rc_adjustment() const { return rc_adjustment; }
      void set_rc_adjustment( const share_type& value ) { rc_adjustment = value; }

      // RC that were delegated to other accounts
      share_type get_delegated_rc() const { return delegated_rc; }
      void set_delegated_rc( const share_type& value ) { delegated_rc = value; }

      // RC that were borrowed from other accounts
      share_type get_received_rc() const { return received_rc; }
      void set_received_rc( const share_type& value ) { received_rc = value; }

      // Last max RC (for bug catching with RC code)
      share_type get_last_max_rc() const { return last_max_rc; }
      void set_last_max_rc( const share_type& value ) { last_max_rc = value; }

      // Manabar accessors
      util::manabar& get_rc_manabar() { return rc_manabar; }
      const util::manabar& get_rc_manabar() const { return rc_manabar; }

      util::manabar& get_voting_manabar() { return voting_manabar; }
      const util::manabar& get_voting_manabar() const { return voting_manabar; }

      util::manabar& get_downvote_manabar() { return downvote_manabar; }
      const util::manabar& get_downvote_manabar() const { return downvote_manabar; }

    private:
      account_id_type   account_id;               // Links to parent account_object

      util::manabar     voting_manabar;
      util::manabar     downvote_manabar;
      util::manabar     rc_manabar;

      share_type        rc_adjustment;            ///< compensation for account creation fee in form of extra RC
      share_type        delegated_rc;             ///< RC delegated out to other accounts
      share_type        received_rc;              ///< RC delegated to this account
      share_type        last_max_rc;              ///< (for bug catching with RC code)

    CHAINBASE_UNPACK_CONSTRUCTOR(manabars_rc_object);
  };

  typedef multi_index_container<
    manabars_rc_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< manabars_rc_object, manabars_rc_object::id_type, &manabars_rc_object::get_id > >,
      ordered_unique< tag< by_account_id >,
        const_mem_fun< manabars_rc_object, account_id_type, &manabars_rc_object::get_account_id > >
    >,
    multi_index_allocator< manabars_rc_object >
  > manabars_rc_index;

} } // hive::chain

FC_REFLECT( hive::chain::manabars_rc_object,
          (id)(account_id)
          (voting_manabar)(downvote_manabar)(rc_manabar)
          (rc_adjustment)(delegated_rc)(received_rc)(last_max_rc)
        )

CHAINBASE_SET_INDEX_TYPE( hive::chain::manabars_rc_object, hive::chain::manabars_rc_index )
