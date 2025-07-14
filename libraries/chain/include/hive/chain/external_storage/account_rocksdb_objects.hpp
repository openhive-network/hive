#pragma once

#include <hive/utilities/benchmark_dumper.hpp>
#include <hive/chain/account_object.hpp>

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

  const account_object* build( chainbase::database& db );

  account_id_type                         id;

  account_details::recovery               recovery;
  account_details::assets                 assets;
  account_details::manabars_rc            mrc;
  account_details::time                   time;
  account_details::misc                   misc;

  std::vector< delayed_votes_data >       delayed_votes;
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

FC_REFLECT( hive::chain::rocksdb_account_object, (id)(recovery)(assets)(mrc)(time)(misc)(delayed_votes) )
FC_REFLECT( hive::chain::rocksdb_account_object_by_id, (id)(name) )
