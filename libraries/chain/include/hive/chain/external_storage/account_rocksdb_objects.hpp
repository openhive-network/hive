#pragma once

#include <hive/chain/account_object.hpp>
#include <hive/chain/external_storage/allocator_helper.hpp>

#ifndef HIVE_ACCOUNT_ROCKSDB_SPACE_ID
#define HIVE_ACCOUNT_ROCKSDB_SPACE_ID 24
#endif

namespace hive { namespace chain {

enum account_rocksdb_object_types
{
  volatile_account_object_type = ( HIVE_ACCOUNT_ROCKSDB_SPACE_ID << 8 )
};

class volatile_account_object : public object< volatile_account_object_type, volatile_account_object >
{
  CHAINBASE_OBJECT( volatile_account_object );

  public:

    CHAINBASE_DEFAULT_CONSTRUCTOR( volatile_account_object, (shared_delayed_votes) )

    const account_name_type& get_name() const { return misc.name; }
    time_point_sec get_next_vesting_withdrawal() const { return time.next_vesting_withdrawal; }

    void set_old_next_vesting_withdrawal( const time_point_sec& val )
    {
      if( !old_next_vesting_withdrawal )
        old_next_vesting_withdrawal = val;
    }
    std::optional<time_point_sec> get_old_next_vesting_withdrawal() const { return old_next_vesting_withdrawal; }

    account_id_type                         account_id;

    account_details::recovery               recovery;
    account_details::assets                 assets;
    account_details::manabars_rc            mrc;
    account_details::time                   time;
    account_details::misc                   misc;
    account_details::shared_delayed_votes   shared_delayed_votes;

    std::optional<time_point_sec>           old_next_vesting_withdrawal;
    uint32_t                                block_number = 0;

    std::shared_ptr<account_object> read() const;

};

typedef oid_ref< volatile_account_object > volatile_account_id_type;

struct by_block;
struct by_name;
struct by_account_id;
struct by_next_vesting_withdrawal;

typedef multi_index_container<
    volatile_account_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< volatile_account_object, volatile_account_object::id_type, &volatile_account_object::get_id > >,
      ordered_unique< tag< by_account_id >,
        composite_key< volatile_account_object,
          member< volatile_account_object, account_id_type, &volatile_account_object::account_id>,
          const_mem_fun< volatile_account_object, volatile_account_object::id_type, &volatile_account_object::get_id >
        >
      >,
      ordered_unique< tag< by_next_vesting_withdrawal >,
        composite_key< volatile_account_object,
          const_mem_fun< volatile_account_object, time_point_sec, &volatile_account_object::get_next_vesting_withdrawal >,
          const_mem_fun< volatile_account_object, const account_name_type&, &volatile_account_object::get_name >
        > /// composite key by_next_vesting_withdrawal
      >,
      ordered_unique< tag< by_block >,
        composite_key< volatile_account_object,
          member< volatile_account_object, uint32_t, &volatile_account_object::block_number>,
          const_mem_fun< volatile_account_object, volatile_account_object::id_type, &volatile_account_object::get_id >
        >
      >,
      ordered_unique< tag< by_name >,
        composite_key< volatile_account_object,
          const_mem_fun< volatile_account_object, const account_name_type&, &volatile_account_object::get_name >,
          const_mem_fun< volatile_account_object, volatile_account_object::id_type, &volatile_account_object::get_id >
        >
      >
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
                                                      delayed_votes) );
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

} } // hive::chain

FC_REFLECT( hive::chain::volatile_account_object, (id)(account_id)(recovery)(assets)(mrc)(time)(misc)(shared_delayed_votes)(old_next_vesting_withdrawal)(block_number) )
CHAINBASE_SET_INDEX_TYPE( hive::chain::volatile_account_object, hive::chain::volatile_account_index )

FC_REFLECT( hive::chain::rocksdb_account_object, (id)(recovery)(assets)(mrc)(time)(misc)(delayed_votes) )
FC_REFLECT( hive::chain::rocksdb_account_object_by_id, (id)(name) )
FC_REFLECT( hive::chain::rocksdb_account_object_by_next_vesting_withdrawal, (next_vesting_withdrawal)(name) )
