#pragma once

#include <hive/utilities/benchmark_dumper.hpp>
#include <hive/chain/account_object.hpp>

#include <hive/chain/external_storage/allocator_helper.hpp>

namespace hive { namespace chain {

struct accounts_stats
{
  static hive::utilities::benchmark_dumper::account_archive_details_t stats; // note: times should be measured in nanoseconds
};

class rocksdb_account_object
{
  public:

  rocksdb_account_object(){}

  rocksdb_account_object( const account_object& obj );

  template<typename Return_Type>
  Return_Type build( chainbase::database& db )
  {
    if constexpr ( std::is_same_v<Return_Type, const account_object*> )
    {
      return &db.create_no_undo<account_object>(
                      id,
                      recovery,
                      assets,
                      mrc,
                      time,
                      misc,
                      dvw );
    }
    else
    {
      return Return_Type(
        std::make_shared<account_object>(
                          allocator_helper::get_allocator<account_object, account_index>( db ),
                          id,
                          recovery,
                          assets,
                          mrc,
                          time,
                          misc,
                          dvw )
      );
    }
  }

  account_id_type                         id;

  account_details::recovery               recovery;
  account_details::assets                 assets;
  account_details::manabars_rc            mrc;
  account_details::time                   time;
  account_details::misc                   misc;

  account_details::delayed_votes_wrapper  dvw;
};

class rocksdb_account_object_by_id
{
  public:

  rocksdb_account_object_by_id(){}

  rocksdb_account_object_by_id( const account_object& obj );

  account_id_type   id;
  account_name_type name;
};

} } // hive::chain

FC_REFLECT( hive::chain::rocksdb_account_object, (id)(recovery)(assets)(mrc)(time)(misc)(dvw) )
FC_REFLECT( hive::chain::rocksdb_account_object_by_id, (id)(name) )