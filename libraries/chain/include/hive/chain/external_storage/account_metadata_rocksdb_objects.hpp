#pragma once

#include <hive/chain/account_object.hpp>

#include <hive/chain/external_storage/allocator_helper.hpp>

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

    template<typename Return_Type>
    Return_Type build( chainbase::database& db )
    {
      if constexpr ( std::is_same_v<Return_Type, const account_metadata_object*> )
      {
        return &db.create_no_undo<account_metadata_object>(
                            id,
                            account,
                            json_metadata,
                            posting_json_metadata
                          );
      }
      else
      {
        return Return_Type(
          std::make_shared<account_metadata_object>(
                            allocator_helper::get_allocator<account_metadata_object, account_metadata_index>( db ),
                            id,
                            account,
                            json_metadata,
                            posting_json_metadata
                          )
        );
      }
    }
};

} } // hive::chain

FC_REFLECT( hive::chain::rocksdb_account_metadata_object, (id)(account)(json_metadata)(posting_json_metadata) )
