#pragma once

#include <fc/uint128.hpp>

#include <hive/chain/hive_fwd.hpp>
#include <hive/chain/hive_object_types.hpp>

namespace hive { namespace chain {

  class time_object : public object< time_object_type, time_object, std::false_type /* no dynamic alloc */, std::true_type /* enable no undo remove */ >
  {
    CHAINBASE_OBJECT( time_object );
    public:
      template< typename Allocator >
      time_object( allocator< Allocator > a, uint64_t _id,
        account_id_type _account_id, const account_name_type& _name = account_name_type() )
        : id( _id ), account_id( _account_id ), name( _name )
      {}

      // Link to parent account_object
      account_id_type get_account_id() const { return account_id; }
      // Account name (needed for by_next_vesting_withdrawal index to match old sort order)
      const account_name_type& get_name() const { return name; }

      // Tells if account has active power down
      bool has_active_power_down() const { return next_vesting_withdrawal != fc::time_point_sec::maximum(); }

      // Next vesting withdrawal time
      time_point_sec get_next_vesting_withdrawal() const { return next_vesting_withdrawal; }
      void set_next_vesting_withdrawal( const time_point_sec& value ) { next_vesting_withdrawal = value; }

      // HBD seconds (liquid HBD * how long it has been held)
      uint128_t get_hbd_seconds() const { return hbd_seconds; }
      void set_hbd_seconds( const uint128_t& value ) { hbd_seconds = value; }

      // Last time HBD seconds was updated
      time_point_sec get_hbd_seconds_last_update() const { return hbd_seconds_last_update; }
      void set_hbd_seconds_last_update( const time_point_sec& value ) { hbd_seconds_last_update = value; }

      // Used to pay interest at most once per month
      time_point_sec get_hbd_last_interest_payment() const { return hbd_last_interest_payment; }
      void set_hbd_last_interest_payment( const time_point_sec& value ) { hbd_last_interest_payment = value; }

      // Only used by outdated consensus checks - up to HF17
      time_point_sec get_last_account_update() const { return last_account_update; }
      void set_last_account_update( const time_point_sec& value ) { last_account_update = value; }

      // Last post time
      time_point_sec get_last_post() const { return last_post; }
      void set_last_post( const time_point_sec& value ) { last_post = value; }

      // Influenced root comment reward between HF12 and HF17
      time_point_sec get_last_root_post() const { return last_root_post; }
      void set_last_root_post( const time_point_sec& value ) { last_root_post = value; }

      // Last post edit time
      time_point_sec get_last_post_edit() const { return last_post_edit; }
      void set_last_post_edit( const time_point_sec& value ) { last_post_edit = value; }

      // Only used by outdated consensus checks - up to HF26
      time_point_sec get_last_vote_time() const { return last_vote_time; }
      void set_last_vote_time( const time_point_sec& value ) { last_vote_time = value; }

    private:
      account_id_type   account_id;               // Links to parent account_object
      account_name_type name;                     // Account name (for index sort order compatibility)

      uint128_t         hbd_seconds = 0;          ///< liquid HBD * how long it has been held

      time_point_sec    hbd_seconds_last_update;
      time_point_sec    hbd_last_interest_payment;

      time_point_sec    last_account_update;      //(only used by outdated consensus checks - up to HF17)
      time_point_sec    last_post;                //(we could probably remove limit on posting replies)
      time_point_sec    last_root_post;           //influenced root comment reward between HF12 and HF17
      time_point_sec    last_post_edit;           //(we could probably remove limit on post edits)
      time_point_sec    last_vote_time;           //(only used by outdated consensus checks - up to HF26)
      time_point_sec    next_vesting_withdrawal = fc::time_point_sec::maximum(); ///< after every withdrawal this is incremented by 1 week

    CHAINBASE_UNPACK_CONSTRUCTOR(time_object);
  };


  struct by_account_id;

  typedef multi_index_container<
    time_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< time_object, time_object::id_type, &time_object::get_id > >,
      ordered_unique< tag< by_account_id >,
        const_mem_fun< time_object, account_id_type, &time_object::get_account_id > >
    >,
    multi_index_allocator< time_object >
  > time_index;

} } // hive::chain

FC_REFLECT( hive::chain::time_object,
          (id)(account_id)(name)
          (hbd_seconds)
          (hbd_seconds_last_update)(hbd_last_interest_payment)
          (last_account_update)(last_post)(last_root_post)
          (last_post_edit)(last_vote_time)(next_vesting_withdrawal)
        )

CHAINBASE_SET_INDEX_TYPE( hive::chain::time_object, hive::chain::time_index )
