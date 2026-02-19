#pragma once

#include <hive/chain/hive_fwd.hpp>
#include <hive/chain/hive_object_types.hpp>

namespace hive { namespace chain {

  class recovery_object : public object< recovery_object_type, recovery_object, std::false_type /* no dynamic alloc */, std::true_type /* enable no undo remove */ >
  {
    CHAINBASE_OBJECT( recovery_object );
    public:
      template< typename Allocator >
      recovery_object( allocator< Allocator > a, uint64_t _id,
        account_id_type _account_id,
        const account_id_type& _recovery_account = account_id_type() )
        : id( _id ), account_id( _account_id ), recovery_account( _recovery_account )
      {}

      // Link to parent account_object
      account_id_type get_account_id() const { return account_id; }

      // Recovery account accessors
      bool has_recovery_account() const { return recovery_account != account_id_type(); }
      account_id_type get_recovery_account() const { return recovery_account; }
      void set_recovery_account( const account_id_type& new_recovery_account )
      {
        recovery_account = new_recovery_account;
      }

      // Timestamp of last time account owner authority was successfully recovered
      time_point_sec get_last_account_recovery_time() const { return last_account_recovery; }
      void set_last_account_recovery_time( time_point_sec recovery_time )
      {
        last_account_recovery = recovery_time;
      }

      // Time from a current block
      time_point_sec get_block_last_account_recovery_time() const { return block_last_account_recovery; }
      void set_block_last_account_recovery_time( time_point_sec block_recovery_time )
      {
        block_last_account_recovery = block_recovery_time;
      }

    private:
      account_id_type   account_id;               // Links to parent account_object
      account_id_type   recovery_account;
      time_point_sec    last_account_recovery;
      time_point_sec    block_last_account_recovery;

    CHAINBASE_UNPACK_CONSTRUCTOR(recovery_object);
  };



  struct by_account_id;

  typedef multi_index_container<
    recovery_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< recovery_object, recovery_object::id_type, &recovery_object::get_id > >,
      ordered_unique< tag< by_account_id >,
        const_mem_fun< recovery_object, account_id_type, &recovery_object::get_account_id > >
    >,
    multi_index_allocator< recovery_object >
  > recovery_index;

} } // hive::chain

FC_REFLECT( hive::chain::recovery_object,
          (id)(account_id)(recovery_account)(last_account_recovery)(block_last_account_recovery)
        )

CHAINBASE_SET_INDEX_TYPE( hive::chain::recovery_object, hive::chain::recovery_index )
