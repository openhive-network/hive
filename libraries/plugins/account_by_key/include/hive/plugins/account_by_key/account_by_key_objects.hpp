#pragma once
#include <hive/chain/hive_object_types.hpp>

namespace hive { namespace plugins { namespace account_by_key {

using namespace std;
using namespace hive::chain;

using hive::protocol::public_key_type;
using hive::protocol::account_name_type;

#ifndef HIVE_ACCOUNT_BY_KEY_SPACE_ID
#define HIVE_ACCOUNT_BY_KEY_SPACE_ID 11
#endif

enum account_by_key_object_types
{
  key_lookup_object_type = ( HIVE_ACCOUNT_BY_KEY_SPACE_ID << 8 )
};

class key_lookup_object : public object< key_lookup_object_type, key_lookup_object >
{
  CHAINBASE_OBJECT( key_lookup_object );
  public:
    CHAINBASE_DEFAULT_CONSTRUCTOR( key_lookup_object )

    public_key_type   key;
    account_name_type account;
};

typedef oid_ref< key_lookup_object > key_lookup_id_type;

struct by_key;

typedef multi_index_container<
  key_lookup_object,
  indexed_by<
    ordered_unique< tag< by_id >,
      const_mem_fun< key_lookup_object, key_lookup_object::id_type, &key_lookup_object::get_id > >,
    ordered_unique< tag< by_key >,
      composite_key< key_lookup_object,
        member< key_lookup_object, public_key_type, &key_lookup_object::key >,
        member< key_lookup_object, account_name_type, &key_lookup_object::account >
      >
    >
  >,
  multi_index_allocator< key_lookup_object >
> key_lookup_index;

} } } // hive::plugins::account_by_key


FC_REFLECT( hive::plugins::account_by_key::key_lookup_object, (id)(key)(account) )
CHAINBASE_SET_INDEX_TYPE( hive::plugins::account_by_key::key_lookup_object, hive::plugins::account_by_key::key_lookup_index )
