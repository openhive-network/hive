#pragma once

#include <hive/chain/account_object.hpp>
#include <hive/chain/account_object_multiindex.hpp>

#include <hive/chain/external_storage/allocator_helper.hpp>

namespace hive { namespace chain {

class rocksdb_account_metadata_object
{
  public:

    rocksdb_account_metadata_object(){}

    rocksdb_account_metadata_object( const account_metadata_object& obj );

    account_metadata_id_type  id;
    account_id_type           account;  // In split architecture, account is id, not name
    std::string               json_metadata;
    std::string               posting_json_metadata;

    template<typename Return_Type>
    Return_Type build( chainbase::database& db )
    {
      if constexpr ( std::is_same_v<Return_Type, const account_metadata_object*> )
      {
        // Note: create_no_undo internally adds allocator, so we only pass (id, lambda)
        return &db.create_no_undo<account_metadata_object>(
                            id.get_value(),
                            [this]( account_metadata_object& obj )
                            {
                              obj.account = this->account;
                              obj.json_metadata = this->json_metadata.c_str();
                              obj.posting_json_metadata = this->posting_json_metadata.c_str();
                            }
                          );
      }
      else
      {
        auto obj = std::make_shared<account_metadata_object>(
                            allocator_helper::get_allocator<account_metadata_object, account_metadata_index>( db ),
                            id.get_value() );
        obj->account = this->account;
        return Return_Type( obj );
      }
    }
};

} } // hive::chain

FC_REFLECT( hive::chain::rocksdb_account_metadata_object, (id)(account)(json_metadata)(posting_json_metadata) )
