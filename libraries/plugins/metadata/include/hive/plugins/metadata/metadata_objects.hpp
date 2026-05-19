#pragma once
#include <hive/chain/hive_fwd.hpp>

#include <hive/chain/hive_object_types.hpp>

namespace hive { namespace plugins { namespace metadata {

using namespace hive::chain;

class account_metadata_object : public object< account_metadata_object_type, account_metadata_object, std::true_type >
{
  CHAINBASE_OBJECT( account_metadata_object );
public:
  CHAINBASE_DEFAULT_CONSTRUCTOR( account_metadata_object, (json_metadata)(posting_json_metadata) )

  account_id_type   account;
  shared_string     json_metadata;
  shared_string     posting_json_metadata;

  size_t get_dynamic_alloc() const
  {
    size_t size = 0;
    size += json_metadata.capacity() * sizeof( decltype( json_metadata )::value_type );
    size += posting_json_metadata.capacity() * sizeof( decltype( posting_json_metadata )::value_type );
    return size;
  }

  CHAINBASE_UNPACK_CONSTRUCTOR( account_metadata_object, (json_metadata)(posting_json_metadata) );
};

typedef oid_ref<account_metadata_object> account_metadata_id_type;

}}}

FC_REFLECT( hive::plugins::metadata::account_metadata_object,
          (id)(account)(json_metadata)(posting_json_metadata) )
