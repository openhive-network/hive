#include <hive/chain/external_storage/account_metadata_rocksdb_objects.hpp>

namespace hive { namespace chain {

rocksdb_account_metadata_object::rocksdb_account_metadata_object( const account_metadata_object& obj )
{
  id                    = obj.get_id();
  account               = obj.account;
  json_metadata         = obj.json_metadata.c_str();
  posting_json_metadata = obj.posting_json_metadata.c_str();
}

const account_metadata_object* rocksdb_account_metadata_object::build( chainbase::database& db )
{
  return &db.create_no_undo<account_metadata_object>(
                      id,
                      account,
                      json_metadata,
                      posting_json_metadata
                    );
}

} } // hive::chain
