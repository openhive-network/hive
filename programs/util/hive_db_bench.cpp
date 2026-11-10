// RocksDB's db_bench, able to open the RocksDB databases hived writes, so traces recorded by hived
// (rocksdb_trace, doc/devs/rocksdb-trace-replay.md) replay on them with -benchmarks=replay.
//
// Stock db_bench cannot open them: it names the column families it opens column_family_name_<N>,
// and hived's column families have their own names and comparators (hive::chain::HashComparator
// and others) which RocksDB does not know. Here hive's comparators are registered with RocksDB's
// object registry, and when db_bench opens an existing database with column families
// (-num_column_families=<the database's count>), the database's own column families are opened,
// each with the comparator its OPTIONS file names and with the options benchmarked.

#include <hive/chain/external_storage/types.hpp>

#include <rocksdb/convenience.h>
#include <rocksdb/db_bench_tool.h>
#include <rocksdb/tool_hooks.h>
#include <rocksdb/utilities/options_util.h>

#include <string>
#include <vector>

namespace {

using ::rocksdb::ColumnFamilyDescriptor;
using ::rocksdb::ColumnFamilyHandle;
using ::rocksdb::DB;
using ::rocksdb::DBOptions;
using ::rocksdb::Options;
using ::rocksdb::Status;

/// The database's own column families, each with @p requested's options (db_bench gives them all
/// the same) and its stored comparator. Leaves @p result empty for a database without an OPTIONS
/// file (one db_bench creates), whose column families db_bench names itself.
Status own_column_families( const DBOptions& db_options, const std::string& db_path,
  const std::vector<ColumnFamilyDescriptor>& requested, std::vector<ColumnFamilyDescriptor>* result )
{
  ::rocksdb::ConfigOptions config_options;
  config_options.env = db_options.env;
  DBOptions stored_db_options;
  std::vector<ColumnFamilyDescriptor> stored;
  const Status s = ::rocksdb::LoadLatestOptions( config_options, db_path, &stored_db_options, &stored );
  if( s.IsNotFound() )
    return Status::OK();
  if( !s.ok() )
    return s;

  if( stored.size() != requested.size() )
    return Status::InvalidArgument( db_path + " has " + std::to_string( stored.size() ) +
      " column families, run with -num_column_families=" + std::to_string( stored.size() ) );

  for( const auto& column_family : stored )
  {
    result->emplace_back( column_family.name, requested.front().options );
    result->back().options.comparator = column_family.options.comparator;
  }
  return Status::OK();
}

class hive_hooks : public ::rocksdb::DefaultHooks
{
  public:

    Status Open( const DBOptions& db_options, const std::string& name,
      const std::vector<ColumnFamilyDescriptor>& column_families,
      std::vector<ColumnFamilyHandle*>* handles, std::unique_ptr<DB>* dbptr ) override
    {
      std::vector<ColumnFamilyDescriptor> own;
      const Status s = own_column_families( db_options, name, column_families, &own );
      if( !s.ok() )
        return s;
      return DefaultHooks::Open( db_options, name, own.empty() ? column_families : own, handles, dbptr );
    }

    Status OpenForReadOnly( const Options& options, const std::string& name,
      const std::vector<ColumnFamilyDescriptor>& column_families,
      std::vector<ColumnFamilyHandle*>* handles, std::unique_ptr<DB>* dbptr ) override
    {
      std::vector<ColumnFamilyDescriptor> own;
      const Status s = own_column_families( options, name, column_families, &own );
      if( !s.ok() )
        return s;
      return DefaultHooks::OpenForReadOnly( options, name, own.empty() ? column_families : own, handles, dbptr );
    }

    using DefaultHooks::Open;
    using DefaultHooks::OpenForReadOnly;
};

}

int main( int argc, char** argv )
{
  hive::chain::registerHiveComparators();
  hive_hooks hooks;
  return ::rocksdb::db_bench_tool( argc, argv, hooks );
}
