#pragma once

#include <hive/chain/account_object.hpp>
#include <hive/chain/account_object_multiindex.hpp>

#include <hive/chain/external_storage/allocator_helper.hpp>

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

    template<typename Return_Type>
    Return_Type build( chainbase::database& db )
    {
      if constexpr ( std::is_same_v<Return_Type, const account_authority_object*> )
      {
        // Note: create_no_undo internally adds allocator, so we only pass (id, lambda)
        return &db.create_no_undo<account_authority_object>(
                            id.get_value(),
                            [this]( account_authority_object& obj )
                            {
                              obj.account = this->account;
                              obj.owner = this->owner;
                              obj.active = this->active;
                              obj.posting = this->posting;
                              obj.previous_owner_update = this->previous_owner_update;
                              obj.last_owner_update = this->last_owner_update;
                            }
                          );
      }
      else
      {
        auto obj = std::make_shared<account_authority_object>(
                            allocator_helper::get_allocator<account_authority_object, account_authority_index>( db ),
                            id.get_value() );
        obj->account = this->account;
        obj->owner = this->owner;
        obj->active = this->active;
        obj->posting = this->posting;
        obj->previous_owner_update = this->previous_owner_update;
        obj->last_owner_update = this->last_owner_update;
        return Return_Type( obj );
      }
    }
};

} } // hive::chain

FC_REFLECT( hive::chain::rocksdb_account_authority_object, (id)(account)(owner)(active)(posting)(previous_owner_update)(last_owner_update) )
