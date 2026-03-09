#pragma once

#include <hive/chain/hive_fwd.hpp>
#include <hive/chain/hive_object_types.hpp>

namespace hive { namespace chain {

  class account_object;
  class account_details_object;

  class tiny_account_object : public object< tiny_account_object_type, tiny_account_object, std::false_type /* no dynamic alloc */ >
  {
    CHAINBASE_OBJECT( tiny_account_object );
    public:

      template< typename Allocator >
      tiny_account_object( allocator< Allocator > a, uint64_t _id,
        const account_object& acc, const account_details_object& details_obj );

      const account_name_type& get_name() const { return name; }

      // ===== Proxy =====
      account_id_type get_proxy() const { return proxy; }
      bool has_proxy() const { return proxy != account_id_type(); }
      void clear_proxy() { proxy = account_id_type(); }
      void set_proxy_by_id( const account_id_type& proxy_id ) { proxy = proxy_id; }

      // ===== Vesting withdrawal =====
      time_point_sec get_next_vesting_withdrawal() const { return next_vesting_withdrawal; }
      void set_next_vesting_withdrawal( const time_point_sec& value ) { next_vesting_withdrawal = value; }
      bool has_active_power_down() const { return next_vesting_withdrawal != fc::time_point_sec::maximum(); }

      // ===== Governance vote expiration =====
      time_point_sec get_governance_vote_expiration_ts() const { return governance_vote_expiration_ts; }
      void set_governance_vote_expired() { governance_vote_expiration_ts = time_point_sec::maximum(); }
      void update_governance_vote_expiration_ts( const time_point_sec vote_time ); // impl in database_account.cpp
      void restore_governance_vote_expiration_ts( const time_point_sec ts ) { governance_vote_expiration_ts = ts; }

      // ===== Delayed votes =====
      bool has_delayed_votes() const { return oldest_delayed_vote_time != time_point_sec::maximum(); }
      time_point_sec get_oldest_delayed_vote_time() const { return oldest_delayed_vote_time; }
      void set_oldest_delayed_vote_time( const time_point_sec& value ) { oldest_delayed_vote_time = value; }

      void modify_from_delayed_votes( const account_details_object& account_details );

    private:
      account_name_type     name;
      account_id_type       proxy;
      time_point_sec        next_vesting_withdrawal = fc::time_point_sec::maximum();
      time_point_sec        governance_vote_expiration_ts = fc::time_point_sec::maximum();
      time_point_sec        oldest_delayed_vote_time = fc::time_point_sec::maximum();

    CHAINBASE_UNPACK_CONSTRUCTOR(tiny_account_object);
  };

  struct by_proxy;
  struct by_next_vesting_withdrawal;
  struct by_delayed_voting;
  struct by_governance_vote_expiration_ts;

  typedef multi_index_container<
    tiny_account_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< tiny_account_object, tiny_account_object::id_type, &tiny_account_object::get_id > >,
      ordered_unique< tag< by_name >,
        const_mem_fun< tiny_account_object, const account_name_type&, &tiny_account_object::get_name > >,
      ordered_unique< tag< by_proxy >,
        composite_key< tiny_account_object,
          const_mem_fun< tiny_account_object, account_id_type, &tiny_account_object::get_proxy >,
          const_mem_fun< tiny_account_object, const account_name_type&, &tiny_account_object::get_name >
        >
      >,
      ordered_unique< tag< by_next_vesting_withdrawal >,
        composite_key< tiny_account_object,
          const_mem_fun< tiny_account_object, time_point_sec, &tiny_account_object::get_next_vesting_withdrawal >,
          const_mem_fun< tiny_account_object, const account_name_type&, &tiny_account_object::get_name >
        >
      >,
      ordered_unique< tag< by_delayed_voting >,
        composite_key< tiny_account_object,
          const_mem_fun< tiny_account_object, time_point_sec, &tiny_account_object::get_oldest_delayed_vote_time >,
          const_mem_fun< tiny_account_object, tiny_account_object::id_type, &tiny_account_object::get_id >
        >
      >,
      ordered_unique< tag< by_governance_vote_expiration_ts >,
        composite_key< tiny_account_object,
          const_mem_fun< tiny_account_object, time_point_sec, &tiny_account_object::get_governance_vote_expiration_ts >,
          const_mem_fun< tiny_account_object, tiny_account_object::id_type, &tiny_account_object::get_id >
        >
      >
    >,
    multi_index_allocator< tiny_account_object >
  > tiny_account_index;

} } // hive::chain

FC_REFLECT( hive::chain::tiny_account_object,
          (id)(name)(proxy)
          (next_vesting_withdrawal)(governance_vote_expiration_ts)
          (oldest_delayed_vote_time)
        )

CHAINBASE_SET_INDEX_TYPE( hive::chain::tiny_account_object, hive::chain::tiny_account_index )
