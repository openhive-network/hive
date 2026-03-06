#pragma once

#include <hive/chain/hive_fwd.hpp>
#include <hive/chain/hive_object_types.hpp>

namespace hive { namespace chain {

  class account_object;
  class assets_object;
  class delayed_votes_object;

  class tiny_account_object : public object< tiny_account_object_type, tiny_account_object, std::false_type /* no dynamic alloc */ >
  {
    CHAINBASE_OBJECT( tiny_account_object );
    public:

      template< typename Allocator >
      tiny_account_object( allocator< Allocator > a, uint64_t _id,
        const account_object& acc, const assets_object& assets_obj, const delayed_votes_object& dvotes );

      const account_name_type& get_name() const { return name; }
      account_id_type get_proxy() const { return proxy; }
      bool has_proxy() const { return proxy != account_id_type(); }
      time_point_sec get_next_vesting_withdrawal() const { return next_vesting_withdrawal; }
      void set_next_vesting_withdrawal( const time_point_sec& value ) { next_vesting_withdrawal = value; }
      bool has_active_power_down() const { return next_vesting_withdrawal != fc::time_point_sec::maximum(); }
      time_point_sec get_governance_vote_expiration_ts() const { return governance_vote_expiration_ts; }
      bool has_delayed_votes() const { return oldest_delayed_vote_time != time_point_sec::maximum(); }

      time_point_sec get_oldest_delayed_vote_time() const { return oldest_delayed_vote_time; }
      void set_oldest_delayed_vote_time( const time_point_sec& value ) { oldest_delayed_vote_time = value; }

      void modify_from_account( const account_object& acc );
      void modify_from_delayed_votes( const delayed_votes_object& dvotes );

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
