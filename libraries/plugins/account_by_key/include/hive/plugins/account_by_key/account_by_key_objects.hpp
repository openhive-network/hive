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

} } } // hive::plugins::account_by_key


FC_REFLECT( hive::plugins::account_by_key::key_lookup_object, (id)(key)(account) )
