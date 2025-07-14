#pragma once

#include <hive/chain/account_object.hpp>

namespace hive { namespace chain {

class rocksdb_account_authority_object
{
  public:

    account_authority_id_type  id;

    account_name_type account;

    authority  owner;
    authority  active;
    authority  posting;

    time_point_sec    previous_owner_update;
    time_point_sec    last_owner_update;

    rocksdb_account_authority_object(){}

    rocksdb_account_authority_object( const account_authority_object& obj );

    const account_authority_object* build( chainbase::database& db );
};

} } // hive::chain

FC_REFLECT( hive::chain::rocksdb_account_authority_object, (id)(account)(owner)(active)(posting)(previous_owner_update)(last_owner_update) )
