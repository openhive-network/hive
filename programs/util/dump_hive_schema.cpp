#include <appbase/application.hpp>

#include <hive/protocol/types_fwd.hpp>
#include <hive/protocol/schema_types/account_name_type.hpp>
#include <hive/protocol/schema_types/asset_symbol_type.hpp>

#include <hive/schema/schema.hpp>
#include <hive/schema/schema_impl.hpp>
#include <hive/schema/schema_types.hpp>

#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <hive/chain/account_object.hpp>
#include <hive/chain/block_storage_interface.hpp>
#include <hive/chain/sync_block_writer.hpp>
#include <hive/chain/hive_objects.hpp>
#include <hive/chain/database.hpp>
#include <hive/chain/index.hpp>
#include <hive/chain/blockchain_worker_thread_pool.hpp>

using hive::schema::abstract_schema;

struct schema_info
{
  schema_info( std::shared_ptr< abstract_schema > s )
  {
    std::vector< std::shared_ptr< abstract_schema > > dep_schemas;
    s->get_deps( dep_schemas );
    for( const std::shared_ptr< abstract_schema >& ds : dep_schemas )
    {
      deps.emplace_back();
      ds->get_name( deps.back() );
    }
    std::string str_schema;
    s->get_str_schema( str_schema );
    schema = fc::json::from_string( str_schema, fc::json::format_validation_mode::full );
  }

  std::vector< std::string >   deps;
  fc::variant                  schema;
};

void add_to_schema_map(
  std::map< std::string, schema_info >& m,
  std::shared_ptr< abstract_schema > schema,
  bool follow_deps = true )
{
  std::string name;
  schema->get_name( name );

  if( m.find( name ) != m.end() )
    return;
  // TODO:  Use iterative, not recursive, algorithm
  m.emplace( name, schema );

  if( !follow_deps )
    return;

  std::vector< std::shared_ptr< abstract_schema > > dep_schemas;
  schema->get_deps( dep_schemas );
  for( const std::shared_ptr< abstract_schema >& ds : dep_schemas )
    add_to_schema_map( m, ds, follow_deps );
}

struct hive_schema
{
  std::map< std::string, schema_info >     schema_map;
  std::vector< std::string >               chain_object_types;
};

FC_REFLECT( schema_info, (deps)(schema) )
FC_REFLECT( hive_schema, (schema_map)(chain_object_types) )

int main( int argc, char** argv, char** envp )
{
  appbase::application app;
  hive::chain::blockchain_worker_thread_pool thread_pool = hive::chain::blockchain_worker_thread_pool( app );

  hive::chain::database db( app );

  std::shared_ptr< hive::chain::block_storage_i > block_storage =
    hive::chain::block_storage_i::create_storage( LEGACY_SINGLE_FILE_BLOCK_LOG, app, thread_pool );
  hive::chain::sync_block_writer block_writer( *(block_storage.get()), db, app );
  db.set_block_writer( &block_writer );

  hive::chain::open_args db_args;
  hive::chain::block_storage_i::block_log_open_args bl_args;

  db_args.data_dir = "tempdata";
  db_args.shared_mem_dir = "tempdata/blockchain";
  db_args.shared_file_size = 1024*1024*8;

  bl_args.data_dir = db_args.data_dir;

  std::map< std::string, schema_info > schema_map;

  db.with_write_lock([&]()
  {
    block_storage->open_and_init( bl_args, true /*read_only*/ );
  });
  block_writer.open();

  db.open( db_args );

  hive_schema ss;

  std::vector< std::string > chain_objects;
  /*
  db.for_each_index_extension< hive::chain::index_info >(
    [&]( std::shared_ptr< hive::chain::index_info > info )
    {
      std::string name;
      info->get_schema()->get_name( name );
      // std::cout << name << std::endl;

      add_to_schema_map( ss.schema_map, info->get_schema() );
      ss.chain_object_types.push_back( name );
    } );
  */
  add_to_schema_map( ss.schema_map, hive::schema::get_schema_for_type< hive::protocol::signed_block >() );

  std::cout << fc::json::to_string( ss ) << std::endl;

  db.close();
  block_writer.close();
  block_storage->close_storage();


  return 0;
}
