#pragma once

#include <hive/chain/account_object.hpp>

#include <hive/utilities/benchmark_dumper.hpp>

#include <hive/chain/external_storage/allocator_helper.hpp>
#include <hive/chain/external_storage/utilities.hpp>

#include <hive/chain/external_storage/custom_cache.hpp>

#ifndef HIVE_ACCOUNT_ROCKSDB_SPACE_ID
#define HIVE_ACCOUNT_ROCKSDB_SPACE_ID 24
#endif

namespace hive { namespace chain {

struct accounts_stats
{
  static hive::utilities::benchmark_dumper::account_archive_details_t stats; // note: times should be measured in nanoseconds
};

enum account_rocksdb_object_types
{
  volatile_account_object_type = ( HIVE_ACCOUNT_ROCKSDB_SPACE_ID << 8 )
};

class volatile_account_object : public object< volatile_account_object_type, volatile_account_object >
{
  CHAINBASE_OBJECT( volatile_account_object );

  public:

    CHAINBASE_DEFAULT_CONSTRUCTOR( volatile_account_object, (shared_delayed_votes) )

    const account_id_type& get_account_id() const { return account_id; }
    const account_name_type& get_name() const { return misc.name; }

    time_point_sec get_next_vesting_withdrawal() const { return time.next_vesting_withdrawal; }
    time_point_sec get_oldest_delayed_vote_time() const { return shared_delayed_votes.get_oldest_delayed_vote_time(); }
    time_point_sec get_governance_vote_expiration_ts() const { return misc.governance_vote_expiration_ts; }

    void set_previous_next_vesting_withdrawal( const time_point_sec& val )
    {
      if( !previous_next_vesting_withdrawal )
        previous_next_vesting_withdrawal = val;
    }
    std::optional<time_point_sec> get_previous_next_vesting_withdrawal() const { return previous_next_vesting_withdrawal; }

    void set_previous_oldest_delayed_vote_time( const time_point_sec& val )
    {
      if( !previous_oldest_delayed_vote_time )
        previous_oldest_delayed_vote_time = val;
    }
    std::optional<time_point_sec> get_previous_oldest_delayed_vote_time() const { return previous_oldest_delayed_vote_time; }

    void set_previous_governance_vote_expiration_ts( const time_point_sec& val )
    {
      if( !previous_governance_vote_expiration_ts )
        previous_governance_vote_expiration_ts = val;
    }
    std::optional<time_point_sec> get_previous_governance_vote_expiration_ts() const { return previous_governance_vote_expiration_ts; }

    account_id_type                         account_id;

    account_details::recovery               recovery;
    account_details::assets                 assets;
    account_details::manabars_rc            mrc;
    account_details::time                   time;
    account_details::misc                   misc;
    account_details::shared_delayed_votes   shared_delayed_votes;

    std::optional<time_point_sec>           previous_next_vesting_withdrawal;
    std::optional<time_point_sec>           previous_oldest_delayed_vote_time;
    std::optional<time_point_sec>           previous_governance_vote_expiration_ts;

    uint32_t                                block_number = 0;

    template<typename Deleter_Type>
    std::shared_ptr<account_object> read() const
    {
      return std::shared_ptr<account_object>( new account_object(
                                                      account_id,
                                                      recovery,
                                                      assets,
                                                      mrc,
                                                      time,
                                                      misc,
                                                      shared_delayed_votes), Deleter_Type() );
    }

};

typedef oid_ref< volatile_account_object > volatile_account_id_type;

struct by_block;
struct by_name;
struct by_account_id;
struct by_next_vesting_withdrawal;
struct by_delayed_voting;

typedef multi_index_container<
    volatile_account_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< volatile_account_object, volatile_account_object::id_type, &volatile_account_object::get_id > >,
      ordered_unique< tag< by_account_id >,
        const_mem_fun< volatile_account_object, const account_id_type&, &volatile_account_object::get_account_id > >,
      ordered_unique< tag< by_next_vesting_withdrawal >,
        composite_key< volatile_account_object,
          const_mem_fun< volatile_account_object, time_point_sec, &volatile_account_object::get_next_vesting_withdrawal >,
          const_mem_fun< volatile_account_object, const account_name_type&, &volatile_account_object::get_name >
        >
      >,
      ordered_unique< tag< by_delayed_voting >,
        composite_key< volatile_account_object,
          const_mem_fun< volatile_account_object, time_point_sec, &volatile_account_object::get_oldest_delayed_vote_time >,
          const_mem_fun< volatile_account_object, const account_id_type&, &volatile_account_object::get_account_id >
        >
      >,
      ordered_unique< tag< by_governance_vote_expiration_ts >,
        composite_key< volatile_account_object,
          const_mem_fun< volatile_account_object, time_point_sec, &volatile_account_object::get_governance_vote_expiration_ts >,
          const_mem_fun< volatile_account_object, const account_id_type&, &volatile_account_object::get_account_id >
        >
      >,
      ordered_unique< tag< by_block >,
        composite_key< volatile_account_object,
          member< volatile_account_object, uint32_t, &volatile_account_object::block_number>,
          const_mem_fun< volatile_account_object, const account_id_type&, &volatile_account_object::get_account_id >
        >
      >,
      ordered_unique< tag< by_name >,
        const_mem_fun< volatile_account_object, const account_name_type&, &volatile_account_object::get_name > >
    >,
    multi_index_allocator< volatile_account_object >
  > volatile_account_index;

class rocksdb_account_object
{
  public:

  rocksdb_account_object(){}

  rocksdb_account_object( const volatile_account_object& obj )
  {
    id          = obj.account_id;

    recovery    = obj.recovery;
    assets      = obj.assets;
    mrc         = obj.mrc;
    time        = obj.time;
    misc        = obj.misc;

    delayed_votes.resize( obj.shared_delayed_votes.delayed_votes.size() );
    for( auto& item : delayed_votes )
    {
      delayed_votes.push_back( item );
    }
  }

  template<typename Deleter_Type>
  std::shared_ptr<account_object> build( const chainbase::database& db )
  {
    return std::shared_ptr<account_object>( new account_object(
                                                      allocator_helper::get_allocator<account_object, account_index>( db ),
                                                      id,
                                                      recovery,
                                                      assets,
                                                      mrc,
                                                      time,
                                                      misc,
                                                      delayed_votes), Deleter_Type() );
  }

  account_id_type                         id;

  account_details::recovery               recovery;
  account_details::assets                 assets;
  account_details::manabars_rc            mrc;
  account_details::time                   time;
  account_details::misc                   misc;

  std::vector< delayed_votes_data >       delayed_votes;
};

class rocksdb_account_object_by_id
{
  public:

  rocksdb_account_object_by_id(){}

  rocksdb_account_object_by_id( const volatile_account_object& obj )
  {
    id   = obj.account_id;
    name = obj.get_name();
  }

  account_id_type   id;
  account_name_type name;
};

class rocksdb_account_object_by_next_vesting_withdrawal
{
  public:

  rocksdb_account_object_by_next_vesting_withdrawal(){}

  rocksdb_account_object_by_next_vesting_withdrawal( const volatile_account_object& obj )
  {
    next_vesting_withdrawal = obj.get_next_vesting_withdrawal();
    name                    = obj.get_name();
  }

  time_point_sec    next_vesting_withdrawal;
  account_name_type name;
};

class rocksdb_account_object_by_delayed_voting
{
  public:

  rocksdb_account_object_by_delayed_voting(){}

  rocksdb_account_object_by_delayed_voting( const volatile_account_object& obj )
  {
    oldest_delayed_vote_time  = obj.get_oldest_delayed_vote_time();
    name                      = obj.get_name();
  }

  time_point_sec    oldest_delayed_vote_time;
  account_id_type   id;
  account_name_type name;
};

class rocksdb_account_object_by_governance_vote_expiration_ts
{
  public:

  rocksdb_account_object_by_governance_vote_expiration_ts(){}

  rocksdb_account_object_by_governance_vote_expiration_ts( const volatile_account_object& obj )
  {
    governance_vote_expiration_ts  = obj.get_governance_vote_expiration_ts();
    name                      = obj.get_name();
  }

  time_point_sec    governance_vote_expiration_ts;
  account_id_type   id;
  account_name_type name;
};

} } // hive::chain

FC_REFLECT( hive::chain::volatile_account_object, (id)(account_id)(recovery)(assets)(mrc)(time)(misc)(shared_delayed_votes)
(previous_next_vesting_withdrawal)(previous_oldest_delayed_vote_time)(previous_governance_vote_expiration_ts)
(block_number) )

CHAINBASE_SET_INDEX_TYPE( hive::chain::volatile_account_object, hive::chain::volatile_account_index )

FC_REFLECT( hive::chain::rocksdb_account_object, (id)(recovery)(assets)(mrc)(time)(misc)(delayed_votes) )
FC_REFLECT( hive::chain::rocksdb_account_object_by_id, (id)(name) )
FC_REFLECT( hive::chain::rocksdb_account_object_by_next_vesting_withdrawal, (next_vesting_withdrawal)(name) )
FC_REFLECT( hive::chain::rocksdb_account_object_by_delayed_voting, (oldest_delayed_vote_time)(id)(name) )
FC_REFLECT( hive::chain::rocksdb_account_object_by_governance_vote_expiration_ts, (governance_vote_expiration_ts)(id)(name) )
