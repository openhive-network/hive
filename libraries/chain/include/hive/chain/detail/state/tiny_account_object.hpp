#pragma once

#include <hive/chain/hive_fwd.hpp>
#include <hive/chain/hive_object_types.hpp>
#include <hive/chain/util/delayed_voting_processor.hpp>

namespace hive { namespace chain {

  class account_object;
  class time_object;
  class delayed_votes_object;

  using chainbase::t_vector;
  using t_tiny_delayed_votes = t_vector< delayed_votes_data >;

  class tiny_account_object : public object< tiny_account_object_type, tiny_account_object, std::true_type /* dynamic alloc */ >
  {
    CHAINBASE_OBJECT( tiny_account_object );
    public:

      template< typename Allocator >
      tiny_account_object( allocator< Allocator > a, uint64_t _id,
        const account_object& acc, const time_object& time_obj, const delayed_votes_object& dvotes );

      const account_name_type& get_name() const { return name; }
      account_id_type get_proxy() const { return proxy; }
      bool has_proxy() const { return proxy != account_id_type(); }
      time_point_sec get_next_vesting_withdrawal() const { return next_vesting_withdrawal; }
      time_point_sec get_governance_vote_expiration_ts() const { return governance_vote_expiration_ts; }
      bool has_delayed_votes() const { return !delayed_votes.empty(); }

      time_point_sec get_oldest_delayed_vote_time() const
      {
        if( has_delayed_votes() )
          return ( delayed_votes.begin() )->time;
        else
          return time_point_sec::maximum();
      }

      void modify_from_account( const account_object& acc );
      void modify_from_time( const time_object& time_obj );
      void modify_from_delayed_votes( const delayed_votes_object& dvotes );

      size_t get_dynamic_alloc() const
      {
        size_t size = 0;
        size += delayed_votes.capacity() * sizeof( decltype( delayed_votes )::value_type );
        return size;
      }

    private:
      account_name_type     name;
      account_id_type       proxy;
      time_point_sec        next_vesting_withdrawal = fc::time_point_sec::maximum();
      time_point_sec        governance_vote_expiration_ts = fc::time_point_sec::maximum();
      t_tiny_delayed_votes  delayed_votes;

      template<typename COLLECTION_TYPE>
      void clone_delayed_votes( const COLLECTION_TYPE& src )
      {
        delayed_votes.clear();
        if( src.size() )
        {
          delayed_votes.reserve( src.size() );
          for( const auto& item : src )
            delayed_votes.push_back( item );
        }
      }

    CHAINBASE_UNPACK_CONSTRUCTOR(tiny_account_object, (delayed_votes));
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
          (delayed_votes)
        )

CHAINBASE_SET_INDEX_TYPE( hive::chain::tiny_account_object, hive::chain::tiny_account_index )
