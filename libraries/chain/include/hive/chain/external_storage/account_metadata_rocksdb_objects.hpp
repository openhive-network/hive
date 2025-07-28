#pragma once

#include <hive/chain/hive_object_types.hpp>
#include <hive/chain/account_object.hpp>

#include <hive/chain/util/type_registrar_definition.hpp>

#ifndef HIVE_ACCOUNT_METADATA_ROCKSDB_SPACE_ID
#define HIVE_ACCOUNT_METADATA_ROCKSDB_SPACE_ID 22
#endif

namespace hive { namespace chain {

enum account_metadata_rocksdb_object_types
{
  volatile_account_metadata_object_type = ( HIVE_ACCOUNT_METADATA_ROCKSDB_SPACE_ID << 8 )
};

class volatile_account_metadata_object : public object< volatile_account_metadata_object_type, volatile_account_metadata_object >
{
  CHAINBASE_OBJECT( volatile_account_metadata_object );

  public:

    CHAINBASE_DEFAULT_CONSTRUCTOR( volatile_account_metadata_object, (json_metadata)(posting_json_metadata) )

    account_metadata_id_type  account_metadata_id;
    account_name_type         account;
    shared_string             json_metadata;
    shared_string             posting_json_metadata;

    uint32_t                  block_number = 0;
};

typedef oid_ref< volatile_account_metadata_object > volatile_account_metadata_id_type;

struct by_block;
struct by_name;

typedef multi_index_container<
    volatile_account_metadata_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< volatile_account_metadata_object, volatile_account_metadata_object::id_type, &volatile_account_metadata_object::get_id > >,
      ordered_unique< tag< by_block >,
        composite_key< volatile_account_metadata_object,
          member< volatile_account_metadata_object, uint32_t, &volatile_account_metadata_object::block_number>,
          const_mem_fun< volatile_account_metadata_object, volatile_account_metadata_object::id_type, &volatile_account_metadata_object::get_id >
        >
      >,
      ordered_unique< tag< by_name >,
        composite_key< volatile_account_metadata_object,
          member< volatile_account_metadata_object, account_name_type, &volatile_account_metadata_object::account>,
          const_mem_fun< volatile_account_metadata_object, volatile_account_metadata_object::id_type, &volatile_account_metadata_object::get_id >
        >
      >
    >,
    multi_index_allocator< volatile_account_metadata_object >
  > volatile_account_metadata_index;

class rocksdb_account_metadata_object
{
  public:

    rocksdb_account_metadata_object(){}

    rocksdb_account_metadata_object( const volatile_account_metadata_object& obj )
    {
      id                    = obj.account_metadata_id;
      account               = obj.account;
      json_metadata         = obj.json_metadata.c_str();
      posting_json_metadata = obj.posting_json_metadata.c_str();
    }

    account_metadata_id_type  id;
    account_name_type         account;
    std::string               json_metadata;
    std::string               posting_json_metadata;
};

} } // hive::chain


FC_REFLECT( hive::chain::volatile_account_metadata_object, (id)(account_metadata_id)(account)(json_metadata)(posting_json_metadata)(block_number) )
CHAINBASE_SET_INDEX_TYPE( hive::chain::volatile_account_metadata_object, hive::chain::volatile_account_metadata_index )

FC_REFLECT( hive::chain::rocksdb_account_metadata_object, (id)(account)(json_metadata)(posting_json_metadata) )
