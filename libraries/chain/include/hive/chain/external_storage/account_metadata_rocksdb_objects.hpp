#pragma once

#include <hive/chain/account_object.hpp>

namespace hive { namespace chain {

class rocksdb_account_metadata_object
{
  public:

    rocksdb_account_metadata_object(){}

    rocksdb_account_metadata_object( const account_metadata_object& obj );

    account_metadata_id_type  id;
    account_name_type         account;
    std::string               json_metadata;
    std::string               posting_json_metadata;

    const account_metadata_object* build( chainbase::database& db );
};

} } // hive::chain

FC_REFLECT( hive::chain::rocksdb_account_metadata_object, (id)(account)(json_metadata)(posting_json_metadata) )
