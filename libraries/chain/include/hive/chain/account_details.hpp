#pragma once

#include <hive/chain/hive_object_types.hpp>

namespace hive { namespace chain { namespace account_details {

  class recovery
  {
    private:

      account_id_type   recovery_account;
      time_point_sec    last_account_recovery;
      time_point_sec    block_last_account_recovery;

    public:

      recovery() {}
      recovery( const account_id_type recovery_account )
              :recovery_account( recovery_account )
      {}

      //tells if account has some designated account that can initiate recovery (if not, top witness can)
      bool has_recovery_account() const { return recovery_account != account_id_type(); }

      //account's recovery account (if any), that is, an account that can authorize request_account_recovery_operation
      account_id_type get_recovery_account() const { return recovery_account; }

      //sets new recovery account
      void set_recovery_account(const account_id_type& new_recovery_account)
      {
        recovery_account = new_recovery_account;
      }

      //timestamp of last time account owner authority was successfully recovered
      time_point_sec get_last_account_recovery_time() const { return last_account_recovery; }

      //sets time of owner authority recovery
      void set_last_account_recovery_time( time_point_sec recovery_time )
      {
        last_account_recovery = recovery_time;
      }

      //time from a current block
      time_point_sec get_block_last_account_recovery_time() const { return block_last_account_recovery; }

      void set_block_last_account_recovery_time( time_point_sec block_recovery_time )
      {
        block_last_account_recovery = block_recovery_time;
      }
  };
}}}

FC_REFLECT( hive::chain::account_details::recovery,
          (recovery_account)(last_account_recovery)(block_last_account_recovery)
        )
