#pragma once

#include "hive/chain/account_details.hpp"
#include <hive/chain/hive_object_types.hpp>
#include <hive/chain/account_object.hpp>

#include <hive/chain/util/type_registrar_definition.hpp>

#ifndef HIVE_ACCOUNT_METADATA_ROCKSDB_SPACE_ID
#define HIVE_ACCOUNT_METADATA_ROCKSDB_SPACE_ID 24
#endif

namespace hive { namespace chain {

enum account_rocksdb_object_types
{
  volatile_account_object_type = ( HIVE_ACCOUNT_METADATA_ROCKSDB_SPACE_ID << 8 )
};

class volatile_account_object : public object< volatile_account_object_type, volatile_account_object >
{
  CHAINBASE_OBJECT( volatile_account_object );

  public:

    CHAINBASE_DEFAULT_CONSTRUCTOR( volatile_account_object, (shared_delayed_votes) )

    account_id_type                         account_id;

    account_details::recovery               recovery;
    account_details::assets                 assets;
    account_details::manabars_rc            mrc;
    account_details::time                   time;
    account_details::misc                   misc;
    account_details::shared_delayed_votes   shared_delayed_votes;

    uint32_t                                block_number = 0;

    const account_name_type& get_name() const { return misc.name; }
};

typedef oid_ref< volatile_account_object > volatile_account_id_type;

struct by_block;
struct by_name;

typedef multi_index_container<
    volatile_account_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< volatile_account_object, volatile_account_object::id_type, &volatile_account_object::get_id > >,
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

using t_rocksdb_delayed_votes = std::vector< delayed_votes_data >;

class rocksdb_account_object
{
  public:

  rocksdb_account_object(){}

  rocksdb_account_object( const volatile_account_object& obj )
  {
    account_id  = obj.account_id;

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

  account_id_type                         account_id;

  account_details::recovery               recovery;
  account_details::assets                 assets;
  account_details::manabars_rc            mrc;
  account_details::time                   time;
  account_details::misc                   misc;

  t_rocksdb_delayed_votes                 delayed_votes;
};

} } // hive::chain

FC_REFLECT( hive::chain::volatile_account_object, (id)(account_id)(recovery)(assets)(mrc)(time)(misc)(shared_delayed_votes)(block_number) )
CHAINBASE_SET_INDEX_TYPE( hive::chain::volatile_account_object, hive::chain::volatile_account_index )

FC_REFLECT( hive::chain::rocksdb_account_object, (account_id)(recovery)(assets)(mrc)(time)(misc)(delayed_votes) )
