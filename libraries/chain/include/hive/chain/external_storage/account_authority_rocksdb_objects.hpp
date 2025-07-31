#pragma once

#include <hive/chain/hive_object_types.hpp>
#include <hive/chain/account_object.hpp>

#include <hive/chain/util/type_registrar_definition.hpp>

#ifndef HIVE_ACCOUNT_AUTHORITY_ROCKSDB_SPACE_ID
#define HIVE_ACCOUNT_AUTHORITY_ROCKSDB_SPACE_ID 23
#endif

namespace hive { namespace chain {

enum account_authority_rocksdb_object_types
{
  volatile_account_authority_object_type = ( HIVE_ACCOUNT_AUTHORITY_ROCKSDB_SPACE_ID << 8 )
};

class volatile_account_authority_object : public object< volatile_account_authority_object_type, volatile_account_authority_object >
{
  CHAINBASE_OBJECT( volatile_account_authority_object );

  public:

    CHAINBASE_DEFAULT_CONSTRUCTOR( volatile_account_authority_object, (owner)(active)(posting) )

    const account_name_type& get_name() const { return account; }

    account_authority_id_type account_authority_id;
    account_name_type         account;

    shared_authority          owner;
    shared_authority          active;
    shared_authority          posting;

    time_point_sec            previous_owner_update;
    time_point_sec            last_owner_update;

    uint32_t                  block_number = 0;
};

typedef oid_ref< volatile_account_authority_object > volatile_account_authority_id_type;

struct by_block;
struct by_name;

typedef multi_index_container<
    volatile_account_authority_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< volatile_account_authority_object, volatile_account_authority_object::id_type, &volatile_account_authority_object::get_id > >,
      ordered_unique< tag< by_block >,
        composite_key< volatile_account_authority_object,
          member< volatile_account_authority_object, uint32_t, &volatile_account_authority_object::block_number>,
          const_mem_fun< volatile_account_authority_object, volatile_account_authority_object::id_type, &volatile_account_authority_object::get_id >
        >
      >,
      ordered_unique< tag< by_name >,
        composite_key< volatile_account_authority_object,
          member< volatile_account_authority_object, account_name_type, &volatile_account_authority_object::account>,
          const_mem_fun< volatile_account_authority_object, volatile_account_authority_object::id_type, &volatile_account_authority_object::get_id >
        >
      >
    >,
    multi_index_allocator< volatile_account_authority_object >
  > volatile_account_authority_index;

class rocksdb_account_authority_object
{
  public:

    rocksdb_account_authority_object(){}

    rocksdb_account_authority_object( const volatile_account_authority_object& obj )
    {
      id                    = obj.account_authority_id;

      account               = obj.account;

      owner                 = obj.owner;
      active                = obj.active;
      posting               = obj.posting;

      previous_owner_update = obj.previous_owner_update;
      last_owner_update     = obj.last_owner_update;
    }

    account_authority_id_type  id;

    account_name_type account;

    authority  owner;
    authority  active;
    authority  posting;

    time_point_sec    previous_owner_update;
    time_point_sec    last_owner_update;
};

} } // hive::chain

FC_REFLECT( hive::chain::volatile_account_authority_object, (id)(account_authority_id)(account)(owner)(active)(posting)(previous_owner_update)(last_owner_update)(block_number) )
CHAINBASE_SET_INDEX_TYPE( hive::chain::volatile_account_authority_object, hive::chain::volatile_account_authority_index )

FC_REFLECT( hive::chain::rocksdb_account_authority_object, (id)(account)(owner)(active)(posting)(previous_owner_update)(last_owner_update) )
