#include <chainbase/chainbase.hpp>

#include <hive/chain/hive_object_types.hpp>
#include <hive/chain/index.hpp>

#include <hive/chain/util/type_registrar_definition.hpp>

#include <hive/chain/external_storage/rocksdb_account_archive.hpp>
#include <hive/chain/external_storage/utilities.hpp>
#include <hive/chain/external_storage/rocksdb_account_storage_provider.hpp>
#include <hive/chain/external_storage/rocksdb_snapshot.hpp>
#include <hive/chain/external_storage/state_snapshot_provider.hpp>

namespace hive { namespace chain {

//#define DBG_INFO
//#define DBG_MOVE_INFO
//#define DBG_MOVE_DETAILS_INFO

hive::utilities::benchmark_dumper::account_archive_details_t accounts_handler::stats;

rocksdb_account_archive::rocksdb_account_archive( database& db, const bfs::path& blockchain_storage_path,
  const bfs::path& storage_path, appbase::application& app, bool destroy_on_startup, bool destroy_on_shutdown )
  : db( db ), destroy_database_on_startup( destroy_on_startup ), destroy_database_on_shutdown( destroy_on_shutdown )
{
  HIVE_ADD_PLUGIN_INDEX( db, volatile_account_metadata_index );
  provider = std::make_shared<rocksdb_account_storage_provider>( blockchain_storage_path, storage_path, app );
  snapshot = std::make_shared<rocksdb_snapshot>( "Accounts RocksDB", "accounts_rocksdb_data", db, storage_path, provider );
}

rocksdb_account_archive::~rocksdb_account_archive()
{
  close();
}

void rocksdb_account_archive::move_to_external_storage_impl( uint32_t block_num, const volatile_account_metadata_object& volatile_object )
{
  auto _account = static_cast<std::string>( volatile_object.account );
  
  Slice _key( _account.data(), _account.size() );

  rocksdb_account_metadata_object _obj( volatile_object );

  auto _serialize_buffer = dump( _obj );
  Slice _value( _serialize_buffer.data(), _serialize_buffer.size() );

  provider->save( _key, _value );
}

template<typename SHM_Object_Type, typename SHM_Object_Index>
auto rocksdb_account_archive::get_allocator() const
{
  auto& _indices = db.get_index<SHM_Object_Index>().indices();
  auto _allocator = _indices.get_allocator();
  return chainbase::get_allocator_helper_t<SHM_Object_Type>::get_generic_allocator( _allocator );
}

template<>
std::shared_ptr<account_metadata_object> rocksdb_account_archive::create<account_metadata_object, account_metadata_index>( const PinnableSlice& buffer ) const
{
  rocksdb_account_metadata_object _obj;

  load( _obj, buffer.data(), buffer.size() );

  return std::shared_ptr<account_metadata_object>( new account_metadata_object(
                                                      get_allocator<account_metadata_object, account_metadata_index>(),
                                                      _obj.id, _obj.account, _obj.json_metadata, _obj.posting_json_metadata ) );
}

template<>
std::shared_ptr<account_authority_object> rocksdb_account_archive::create<account_authority_object, account_authority_index>( const PinnableSlice& buffer ) const
{
  rocksdb_account_authority_object _obj;

  load( _obj, buffer.data(), buffer.size() );

  return std::shared_ptr<account_authority_object>( new account_authority_object(
                                                      get_allocator<account_authority_object, account_authority_index>(),
                                                    _obj.id, _obj.account,
                                                 _obj.owner, _obj.active, _obj.posting,
                                 _obj.previous_owner_update, _obj.last_owner_update) );
}

template<typename SHM_Object_Type, typename SHM_Object_Index>
std::shared_ptr<SHM_Object_Type> rocksdb_account_archive::get_object_impl( const std::string& account_name ) const
{
  Slice _key( account_name.data(), account_name.size() );

  PinnableSlice _buffer;

  bool _status = provider->read( _key, _buffer );

  if( !_status )
    return std::shared_ptr<SHM_Object_Type>();

  return create<SHM_Object_Type, SHM_Object_Index>( _buffer );
}

void rocksdb_account_archive::on_irreversible_block( uint32_t block_num )
{
  provider->update_lib( block_num );

  const auto& _volatile_idx = db.get_index< volatile_account_metadata_index, by_block >();

  if( _volatile_idx.size() < volatile_objects_limit )
    return;

  auto time_start = std::chrono::high_resolution_clock::now();

  auto _itr = _volatile_idx.begin();

#ifdef DBG_MOVE_INFO
  ilog( "rocksdb_account_archive: volatile index size: ${size} for block: ${block_num}", (block_num)("size", _volatile_idx.size()) );
#endif

  bool _do_flush = false;

  uint64_t count = 0;
  while( _itr != _volatile_idx.end() && _itr->block_number <= block_num )
  {
    const auto& _current = *_itr;
    ++_itr;

    move_to_external_storage_impl( block_num, _current );

    if( !_do_flush )
      _do_flush = true;

    const auto* _account_metadata = db.find< account_metadata_object, by_account >( _current.account );
    if( _account_metadata )
      db.remove( *_account_metadata );

    db.remove( _current );

    ++count;
  }

  if( _do_flush )
    provider->flush();

  stats.account_lib_processing.time_ns += std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
  stats.account_lib_processing.count += count;
}

template<typename Object_Type, typename SHM_Object_Type, typename SHM_Object_Index>
Object_Type rocksdb_account_archive::get_object( const std::string& account_name ) const
{
  auto time_start = std::chrono::high_resolution_clock::now();

  const auto* _found = db.find<SHM_Object_Type, by_account>( account_name );
  if( _found )
  {
    stats.account_accessed_from_index.time_ns += std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
    ++stats.account_accessed_from_index.count;
    return Object_Type( _found );
  }
  else
  {
    const auto _external_found = get_object_impl<SHM_Object_Type, SHM_Object_Index>( account_name );
    uint64_t time = std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
    if( _external_found )
    {
      stats.account_accessed_from_archive.time_ns += time;
      ++stats.account_accessed_from_archive.count;
    }
    else
    {
      stats.account_not_found.time_ns += time;
      ++stats.account_not_found.count;
      FC_ASSERT( false, "Account metadata not found" );
    }
    return Object_Type( _external_found );
  }
}

void rocksdb_account_archive::create_volatile_account_metadata( uint32_t block_num, const account_metadata_object& obj )
{
  auto time_start = std::chrono::high_resolution_clock::now();

  db.create< volatile_account_metadata_object >( [&]( volatile_account_metadata_object& o )
  {
    o.account_metadata_id   = obj.get_id();
    o.account               = obj.account;
    o.json_metadata         = obj.json_metadata;
    o.posting_json_metadata = obj.posting_json_metadata;

    o.block_number          = block_num;
  });

  stats.account_cashout_processing.time_ns += std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
  ++stats.account_cashout_processing.count;
}

account_metadata rocksdb_account_archive::get_account_metadata( const std::string& account_name ) const
{
  return get_object<account_metadata, account_metadata_object, account_metadata_index>( account_name );
}

void rocksdb_account_archive::create_volatile_account_authority( uint32_t block_num, const account_authority_object& obj )
{
  auto time_start = std::chrono::high_resolution_clock::now();

  db.create< volatile_account_authority_object >( [&]( volatile_account_authority_object& o )
  {
    o.account_authority_id  = obj.get_id();
    o.account               = obj.account;

    o.owner                 = obj.owner;
    o.active                = obj.active;
    o.posting               = obj.posting;

    o.previous_owner_update = obj.previous_owner_update;
    o.last_owner_update     = obj.last_owner_update;

    o.block_number          = block_num;
  });

  stats.account_cashout_processing.time_ns += std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
  ++stats.account_cashout_processing.count;
}

account_authority rocksdb_account_archive::get_account_authority( const std::string& account_name ) const
{
  return get_object<account_authority, account_authority_object, account_authority_index>( account_name );
}

void rocksdb_account_archive::save_snaphot( const hive::chain::prepare_snapshot_supplement_notification& note )
{
  snapshot->save_snaphot( note );
}

void rocksdb_account_archive::load_snapshot( const hive::chain::load_snapshot_supplement_notification& note )
{
  snapshot->load_snapshot( note );
}

void rocksdb_account_archive::open()
{
  // volatile_account_metadata_index is registered in database, so it is handled automatically
  provider->init( destroy_database_on_startup );
}

void rocksdb_account_archive::close()
{
  // volatile_account_metadata_index is registered in database, so it is handled automatically
  provider->shutdownDb( destroy_database_on_shutdown );
}

void rocksdb_account_archive::wipe()
{
  // volatile_account_metadata_index is registered in database, so it is handled automatically
  provider->wipeDb();
}

} }

HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE( hive::chain::volatile_account_metadata_index )
