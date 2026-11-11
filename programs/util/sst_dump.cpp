// RocksDB's sst_dump, able to read every SST hived writes, including those ordered by hive's own
// comparators (state snapshot index dumps, the comment archive's and account history's column
// families with hive::chain::HashComparator and the PrimitiveTypeComparatorImpl instantiations).
//
// sst_dump resolves the comparator an SST names through RocksDB's object registry, with
// ignore_unsupported_options set: a name it does not know leaves it with no comparator, and it
// crashes. Here hive's comparators are registered, and any other name that nothing knows is
// reported as an error naming it.

#include <hive/chain/external_storage/types.hpp>

#include <rocksdb/sst_dump_tool.h>
#include <rocksdb/utilities/object_registry.h>

#include <memory>
#include <string>

namespace {

using ::rocksdb::Comparator;
using ::rocksdb::ObjectLibrary;

/// Answers the comparator names neither RocksDB's builtin library nor any library added after it
/// knows with an error (InvalidArgument, which sst_dump does not ignore). Libraries are searched
/// last added first, so this must be added before hive's comparators.
void add_unknown_comparator_error()
{
  auto library = std::make_shared<ObjectLibrary>( "hive_sst_dump_unknown_comparator" );
  library->AddFactory<const Comparator>( ObjectLibrary::PatternEntry( "", false ).AddSeparator( "" ),
    []( const std::string& name, std::unique_ptr<const Comparator>* guard, std::string* errmsg ) -> const Comparator*
    {
      const auto builtin = ObjectLibrary::Default()->FindFactory<const Comparator>( name );
      if( builtin )
        return builtin( name, guard, errmsg );
      *errmsg = "sst_dump does not know the SST's comparator";
      return nullptr;
    } );
  ::rocksdb::ObjectRegistry::Default()->AddLibrary( library );
}

}

int main( int argc, char** argv )
{
  add_unknown_comparator_error();
  hive::chain::registerHiveComparators();
  ::rocksdb::SSTDumpTool tool;
  return tool.Run( argc, argv );
}
