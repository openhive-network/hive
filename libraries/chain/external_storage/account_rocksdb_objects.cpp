#include <hive/chain/external_storage/account_rocksdb_objects.hpp>

namespace hive { namespace chain {

hive::utilities::benchmark_dumper::account_archive_details_t accounts_stats::stats;

rocksdb_account_object::rocksdb_account_object( const account_object& obj )
{
  id          = obj.get_id();

  recovery    = obj.get_recovery();
  assets      = obj.get_assets();
  mrc         = obj.get_mrc();
  time        = obj.get_time();
  misc        = obj.get_misc();

  if( obj.get_delayed_votes().size() )
  {
    delayed_votes.reserve( obj.get_delayed_votes().size() );
    for( const auto& item : obj.get_delayed_votes() )
      delayed_votes.push_back( item );
  }
}

const account_object* rocksdb_account_object::build( chainbase::database& db )
{
  return &db.create_no_undo<account_object>(
                  id,
                  recovery,
                  assets,
                  mrc,
                  time,
                  misc,
                  delayed_votes );
}

rocksdb_account_object_by_id::rocksdb_account_object_by_id( const account_object& obj )
{
  id   = obj.get_id();
  name = obj.get_name();
}

} } // hive::chain
