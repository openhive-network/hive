#include <hive/chain/external_storage/account_authority_rocksdb_objects.hpp>

namespace hive { namespace chain {

rocksdb_account_authority_object::rocksdb_account_authority_object( const account_authority_object& obj )
{
  id                    = obj.get_id();

  account               = obj.account;

  owner                 = obj.owner;
  active                = obj.active;
  posting               = obj.posting;

  previous_owner_update = obj.previous_owner_update;
  last_owner_update     = obj.last_owner_update;
}

} } // hive::chain
