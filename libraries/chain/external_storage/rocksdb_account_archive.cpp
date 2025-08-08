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

namespace
{
  template<typename Volatile_Object_Type, typename SHM_Object_Type>
  struct creator
  {
    static void assign( Volatile_Object_Type& dest, const SHM_Object_Type& src, uint32_t block_num ){}
  };

  template<>
  struct creator<volatile_account_metadata_object, account_metadata_object>
  {
    static void assign( volatile_account_metadata_object& dest, const account_metadata_object& src, uint32_t block_num )
    {
      dest.account_metadata_id   = src.get_id();
      dest.account               = src.account;
      dest.json_metadata         = src.json_metadata;
      dest.posting_json_metadata = src.posting_json_metadata;

      dest.block_number          = block_num;
    }
  };

  template<>
  struct creator<volatile_account_authority_object, account_authority_object>
  {
    static void assign( volatile_account_authority_object& dest, const account_authority_object& src, uint32_t block_num )
    {
      dest.account_authority_id  = src.get_id();
      dest.account               = src.account;

      dest.owner                 = src.owner;
      dest.active                = src.active;
      dest.posting               = src.posting;

      dest.previous_owner_update = src.previous_owner_update;
      dest.last_owner_update     = src.last_owner_update;

      dest.block_number          = block_num;
    }
  };

  template<>
  struct creator<volatile_account_object, account_object>
  {
    static void assign( volatile_account_object& dest, const account_object& src, uint32_t block_num )
    {
      dest.account_id  = src.get_id();

      dest.recovery             = src.get_recovery();
      dest.assets               = src.get_assets();
      dest.mrc                  = src.get_mrc();
      dest.time                 = src.get_time();
      dest.misc                 = src.get_misc();
      dest.shared_delayed_votes = src.get_shared_delayed_votes();

      dest.block_number         = block_num;
    }
  };

template<typename Volatile_Object_Type, typename RocksDB_Object_Type, typename Slice_Type, typename Key_Type>
struct transporter_impl
{
  static void move_to_external_storage( rocksdb_account_storage_provider::ptr& provider, const Key_Type& key, uint32_t block_num, const Volatile_Object_Type& volatile_object, ColumnTypes column_type )
  {
    Slice_Type _key( key );

    RocksDB_Object_Type _obj( volatile_object );

    auto _serialize_buffer = dump( _obj );
    Slice _value( _serialize_buffer.data(), _serialize_buffer.size() );

    provider->save( column_type, _key, _value );
  }
};

template<typename Volatile_Object_Type, typename RocksDB_Object_Type, typename RocksDB_Object_Type2, typename RocksDB_Object_Type3>
struct transporter
{
  static void move_to_external_storage( rocksdb_account_storage_provider::ptr& provider, uint32_t block_num, const Volatile_Object_Type& volatile_object, const std::vector<ColumnTypes>& column_types )
  {
    FC_ASSERT( column_types.size() );
    transporter_impl<Volatile_Object_Type, RocksDB_Object_Type, account_name_slice_t, account_name_type::Storage>::move_to_external_storage( provider, volatile_object.get_name().data, block_num, volatile_object, column_types[0] );
  }
};

template<>
struct transporter<volatile_account_object, rocksdb_account_object, rocksdb_account_object_by_id, rocksdb_account_object_by_next_vesting_withdrawal>
{
  static void move_to_external_storage( rocksdb_account_storage_provider::ptr& provider, uint32_t block_num, const volatile_account_object& volatile_object, const std::vector<ColumnTypes>& column_types )
  {
    FC_ASSERT( column_types.size() == 3 );
    transporter_impl<volatile_account_object, rocksdb_account_object, account_name_slice_t, account_name_type::Storage>::move_to_external_storage( provider, volatile_object.get_name().data, block_num, volatile_object, column_types[0] );
    transporter_impl<volatile_account_object, rocksdb_account_object_by_id, uint32_slice_t, uint32_t>::move_to_external_storage( provider, volatile_object.account_id, block_num, volatile_object, column_types[1] );
    transporter_impl<volatile_account_object, rocksdb_account_object_by_next_vesting_withdrawal, uint32_slice_t, uint32_t>::move_to_external_storage( provider, volatile_object.get_next_vesting_withdrawal().sec_since_epoch(), block_num, volatile_object, column_types[2] );
  }
};


struct rocksdb_reader_helper
{
  template<typename SHM_Object_Type, typename SHM_Object_Index>
  static auto get_allocator( database& db )
  {
    auto& _indices = db.get_index<SHM_Object_Index>().indices();
    auto _allocator = _indices.get_allocator();
    return chainbase::get_allocator_helper_t<SHM_Object_Type>::get_generic_allocator( _allocator );
  }

  template<typename Slice_Type, typename Key_Type>
  static bool read( const rocksdb_account_storage_provider::ptr& provider, const Key_Type& key, ColumnTypes column_type, PinnableSlice& buffer )
  {
    Slice_Type _key( key );

    return provider->read( column_type, _key, buffer );
  }

};

template<typename SHM_Object_Type, typename SHM_Object_Index, typename Key_Type>
struct rocksdb_reader
{
};

template<>
struct rocksdb_reader<account_metadata_object, account_metadata_index, account_name_type>
{
  static std::shared_ptr<account_metadata_object> read( database& db, const rocksdb_account_storage_provider::ptr& provider, const account_name_type& key, const std::vector<ColumnTypes>& column_types )
  {
    PinnableSlice _buffer;

    FC_ASSERT( column_types.size() );
    if( !rocksdb_reader_helper::read<account_name_slice_t, account_name_type::Storage>( provider, key.data, column_types[0], _buffer ) )
      return std::shared_ptr<account_metadata_object>();

    rocksdb_account_metadata_object _obj;

    load( _obj, _buffer.data(), _buffer.size() );

    return std::shared_ptr<account_metadata_object>( new account_metadata_object(
                                                        rocksdb_reader_helper::get_allocator<account_metadata_object, account_metadata_index>( db ),
                                                        _obj.id, _obj.account, _obj.json_metadata, _obj.posting_json_metadata ) );
  }
};

template<>
struct rocksdb_reader<account_authority_object, account_authority_index, account_name_type>
{
  static std::shared_ptr<account_authority_object> read( database& db, const rocksdb_account_storage_provider::ptr& provider, const account_name_type& key, const std::vector<ColumnTypes>& column_types )
  {
    PinnableSlice _buffer;

    FC_ASSERT( column_types.size() );
    if( !rocksdb_reader_helper::read<account_name_slice_t, account_name_type::Storage>( provider, key.data, column_types[0], _buffer ) )
      return std::shared_ptr<account_authority_object>();

    rocksdb_account_authority_object _obj;

    load( _obj, _buffer.data(), _buffer.size() );

    return std::shared_ptr<account_authority_object>( new account_authority_object(
                                                        rocksdb_reader_helper::get_allocator<account_authority_object, account_authority_index>( db ),
                                                      _obj.id, _obj.account,
                                                  _obj.owner, _obj.active, _obj.posting,
                                  _obj.previous_owner_update, _obj.last_owner_update) );
  }
};

template<>
struct rocksdb_reader<account_object, account_index, account_name_type>
{
  static std::shared_ptr<account_object> read( database& db, const rocksdb_account_storage_provider::ptr& provider, const account_name_type& key, const std::vector<ColumnTypes>& column_types )
  {
    PinnableSlice _buffer;

    FC_ASSERT( column_types.size() );
    if( !rocksdb_reader_helper::read<account_name_slice_t, account_name_type::Storage>( provider, key.data, column_types[0], _buffer ) )
      return std::shared_ptr<account_object>();

    rocksdb_account_object _obj;

    load( _obj, _buffer.data(), _buffer.size() );

    return std::shared_ptr<account_object>( new account_object(
                                                        rocksdb_reader_helper::get_allocator<account_object, account_index>( db ),
                                                      _obj.id,
                                                      _obj.recovery,
                                                      _obj.assets,
                                                      _obj.mrc,
                                                      _obj.time,
                                                      _obj.misc,
                                                      _obj.delayed_votes) );
  }
};

template<>
struct rocksdb_reader<account_object, account_index, account_id_type>
{
  static std::shared_ptr<account_object> read( database& db, const rocksdb_account_storage_provider::ptr& provider, const account_id_type& key, const std::vector<ColumnTypes>& column_types )
  {
    account_name_type _name;

    FC_ASSERT( column_types.size() == 2 );

    {
      PinnableSlice _buffer;

      if( !rocksdb_reader_helper::read<uint32_slice_t, uint32_t>( provider, key, column_types[0], _buffer ) )
        return std::shared_ptr<account_object>();

      rocksdb_account_object_by_id _obj;
      load( _obj, _buffer.data(), _buffer.size() );
      _name = _obj.name;
    }

    PinnableSlice _buffer;

    if( !rocksdb_reader_helper::read<account_name_slice_t, account_name_type::Storage>( provider, _name.data, column_types[1], _buffer ) )
      return std::shared_ptr<account_object>();

    rocksdb_account_object _obj;
    load( _obj, _buffer.data(), _buffer.size() );

    return std::shared_ptr<account_object>( new account_object(
                                                        rocksdb_reader_helper::get_allocator<account_object, account_index>( db ),
                                                      _obj.id,
                                                      _obj.recovery,
                                                      _obj.assets,
                                                      _obj.mrc,
                                                      _obj.time,
                                                      _obj.misc,
                                                      _obj.delayed_votes) );
  }
};
}

hive::utilities::benchmark_dumper::account_archive_details_t accounts_handler::stats;

rocksdb_account_archive::rocksdb_account_archive( database& db, const bfs::path& blockchain_storage_path,
  const bfs::path& storage_path, appbase::application& app, bool destroy_on_startup, bool destroy_on_shutdown )
  : db( db ), destroy_database_on_startup( destroy_on_startup ), destroy_database_on_shutdown( destroy_on_shutdown )
{
  HIVE_ADD_PLUGIN_INDEX( db, volatile_account_metadata_index );
  HIVE_ADD_PLUGIN_INDEX( db, volatile_account_authority_index );
  HIVE_ADD_PLUGIN_INDEX( db, volatile_account_index );

  provider = std::make_shared<rocksdb_account_storage_provider>( blockchain_storage_path, storage_path, app );
  snapshot = std::make_shared<rocksdb_snapshot>( "Accounts RocksDB", "accounts_rocksdb_data", db, storage_path, provider );
}

rocksdb_account_archive::~rocksdb_account_archive()
{
  close();
}

uint32_t rocksdb_account_archive::get_block_num() const
{
  auto _found_dgpo = db.find< dynamic_global_property_object >();
  return _found_dgpo ? _found_dgpo->head_block_number : 0;
}

template<>
std::shared_ptr<account_metadata_object> rocksdb_account_archive::create_from_volatile_object<volatile_account_metadata_object, account_metadata_object, account_metadata_index>( const volatile_account_metadata_object& obj ) const
{
  return std::shared_ptr<account_metadata_object>( new account_metadata_object(
                                                      rocksdb_reader_helper::get_allocator<account_metadata_object, account_metadata_index>( db ),
                                                      obj.account_metadata_id, obj.account, obj.json_metadata, obj.posting_json_metadata ) );
}

template<>
std::shared_ptr<account_authority_object> rocksdb_account_archive::create_from_volatile_object<volatile_account_authority_object, account_authority_object, account_authority_index>( const volatile_account_authority_object& obj ) const
{
  return std::shared_ptr<account_authority_object>( new account_authority_object(
                                                      rocksdb_reader_helper::get_allocator<account_authority_object, account_authority_index>( db ),
                                                    obj.account_authority_id, obj.account,
                                                 obj.owner, obj.active, obj.posting,
                                 obj.previous_owner_update, obj.last_owner_update) );
}

template<>
std::shared_ptr<account_object> rocksdb_account_archive::create_from_volatile_object<volatile_account_object, account_object, account_index>( const volatile_account_object& obj ) const
{
  return std::shared_ptr<account_object>( new account_object(
                                                    obj.account_id,
                                                    obj.recovery,
                                                    obj.assets,
                                                    obj.mrc,
                                                    obj.time,
                                                    obj.misc,
                                                    obj.shared_delayed_votes) );
}

template<typename Volatile_Index_Type, typename Volatile_Object_Type, typename SHM_Object_Type, typename RocksDB_Object_Type, typename RocksDB_Object_Type2 = RocksDB_Object_Type, typename RocksDB_Object_Type3 = RocksDB_Object_Type>
bool rocksdb_account_archive::on_irreversible_block_impl( uint32_t block_num, const std::vector<ColumnTypes>& column_types )
{
  const auto& _volatile_idx = db.get_index<Volatile_Index_Type, by_block>();

  if( _volatile_idx.size() < volatile_objects_limit )
    return false;

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

    transporter<Volatile_Object_Type, RocksDB_Object_Type, RocksDB_Object_Type2, RocksDB_Object_Type3>::move_to_external_storage( provider, block_num, _current, column_types );

    if( !_do_flush )
      _do_flush = true;

    const auto* _shm_object = db.find< SHM_Object_Type, by_name >( _current.get_name() );
    if( _shm_object )
      db.remove( *_shm_object );

    db.remove( _current );

    ++count;
  }


  stats.account_lib_processing.time_ns += std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
  stats.account_lib_processing.count += count;

  return _do_flush;
}

void rocksdb_account_archive::on_irreversible_block( uint32_t block_num )
{
  provider->update_lib( block_num );

  bool _do_flush_meta = on_irreversible_block_impl<
                          volatile_account_metadata_index, volatile_account_metadata_object, account_metadata_object, rocksdb_account_metadata_object>
                          ( block_num, { ColumnTypes::ACCOUNT_METADATA } );

  bool _do_flush_authority = on_irreversible_block_impl<
                          volatile_account_authority_index, volatile_account_authority_object, account_authority_object, rocksdb_account_authority_object>
                          ( block_num, { ColumnTypes::ACCOUNT_AUTHORITY } );

  bool _do_flush_account = on_irreversible_block_impl
                          <volatile_account_index, volatile_account_object, account_object, rocksdb_account_object, rocksdb_account_object_by_id, rocksdb_account_object_by_next_vesting_withdrawal>
                          ( block_num, { ColumnTypes::ACCOUNT, ColumnTypes::ACCOUNT_BY_ID, ColumnTypes::ACCOUNT_BY_NEXT_VESTING_WITHDRAWAL } );

  if( _do_flush_meta || _do_flush_authority || _do_flush_account )
  {
    provider->flush();
  }
}

template<typename Key_Type, typename Volatile_Object_Type, typename Volatile_Index_Type, typename Volatile_Sub_Index_Type, typename Object_Type, typename SHM_Object_Type, typename SHM_Object_Index, typename SHM_Object_Sub_Index>
Object_Type rocksdb_account_archive::get_object( const Key_Type& key, const std::vector<ColumnTypes>& column_types, bool is_required ) const
{
  auto time_start = std::chrono::high_resolution_clock::now();

  const auto* _found = db.find<SHM_Object_Type, SHM_Object_Sub_Index>( key );
  if( _found )
  {
    stats.account_accessed_from_index.time_ns += std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
    ++stats.account_accessed_from_index.count;
    return Object_Type( _found );
  }
  else
  {
    const auto& _volatile_idx = db.get_index<Volatile_Index_Type, Volatile_Sub_Index_Type>();
    auto _volatile_found = _volatile_idx.find( key );
    if( _volatile_found != _volatile_idx.end() )
    {
      const auto _external_found = create_from_volatile_object<Volatile_Object_Type, SHM_Object_Type, SHM_Object_Index>( *_volatile_found );
      return Object_Type( _external_found );
    }
    else
    {
      const auto _external_found = rocksdb_reader<SHM_Object_Type, SHM_Object_Index, Key_Type>::read( db, provider, key, column_types );
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
        FC_ASSERT( !is_required, "Account data not found" );
      }
      return Object_Type( _external_found );
    }
  }
}

template<typename SHM_Object_Type>
void rocksdb_account_archive::modify( const SHM_Object_Type& obj, std::function<void(SHM_Object_Type&)> modifier )
{
  modifier( const_cast<SHM_Object_Type&>( obj ) );
  create_or_update_volatile( obj );
}

template<typename Volatile_Index_Type, typename Volatile_Object_Type, typename SHM_Object_Type>
void rocksdb_account_archive::create_or_update_volatile_impl( const SHM_Object_Type& obj )
{
  auto time_start = std::chrono::high_resolution_clock::now();

  const auto& _volatile_idx = db.get_index<Volatile_Index_Type, by_name>();
  auto _found = _volatile_idx.find( obj.get_name() );
  if( _found != _volatile_idx.end() )
  {
    db.modify<Volatile_Object_Type>( *_found, [&]( Volatile_Object_Type& o )
    {
      creator<Volatile_Object_Type, SHM_Object_Type>::assign( o, obj, get_block_num() );
    } );
  }
  else
  {
    db.create<Volatile_Object_Type>( [&]( Volatile_Object_Type& o )
    {
      creator<Volatile_Object_Type, SHM_Object_Type>::assign( o, obj, get_block_num() );
    });
  }

  stats.account_cashout_processing.time_ns += std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
  ++stats.account_cashout_processing.count;
}

//==========================================account_metadata_object==========================================
void rocksdb_account_archive::create_or_update_volatile( const account_metadata_object& obj )
{
  create_or_update_volatile_impl<volatile_account_metadata_index, volatile_account_metadata_object>( obj );
}

account_metadata rocksdb_account_archive::get_account_metadata( const account_name_type& account_name ) const
{
  return get_object<account_name_type, volatile_account_metadata_object, volatile_account_metadata_index, by_name, account_metadata, account_metadata_object, account_metadata_index, by_name>( account_name, { ColumnTypes::ACCOUNT_METADATA }, true/*is_required*/ );
}

void rocksdb_account_archive::modify_object( const account_metadata_object& obj, std::function<void(account_metadata_object&)>&& modifier )
{
  modify( obj, modifier );
}
//==========================================account_metadata_object==========================================

//==========================================account_authority_object==========================================
void rocksdb_account_archive::create_or_update_volatile( const account_authority_object& obj )
{
  create_or_update_volatile_impl<volatile_account_authority_index, volatile_account_authority_object>( obj );
}

account_authority rocksdb_account_archive::get_account_authority( const account_name_type& account_name ) const
{
  return get_object<account_name_type, volatile_account_authority_object, volatile_account_authority_index, by_name, account_authority, account_authority_object, account_authority_index, by_name>( account_name, { ColumnTypes::ACCOUNT_AUTHORITY }, true/*is_required*/ );
}

void rocksdb_account_archive::modify_object( const account_authority_object& obj, std::function<void(account_authority_object&)>&& modifier )
{
  modify( obj, modifier );
}
//==========================================account_authority_object==========================================

//==========================================account_object==========================================
void rocksdb_account_archive::create_or_update_volatile( const account_object& obj )
{
  create_or_update_volatile_impl<volatile_account_index, volatile_account_object>( obj );
}

account rocksdb_account_archive::get_account( const account_name_type& account_name, bool account_is_required ) const
{
  return get_object<account_name_type, volatile_account_object, volatile_account_index, by_name, account, account_object, account_index, by_name>( account_name, { ColumnTypes::ACCOUNT }, account_is_required );
}

account rocksdb_account_archive::get_account( const account_id_type& account_id, bool account_is_required ) const
{
  return get_object<account_id_type, volatile_account_object, volatile_account_index, by_account_id, account, account_object, account_index, by_id>( account_id, { ColumnTypes::ACCOUNT_BY_ID, ColumnTypes::ACCOUNT }, account_is_required );
}

void rocksdb_account_archive::modify_object( const account_object& obj, std::function<void(account_object&)>&& modifier )
{
  modify( obj, modifier );
}
//==========================================account_object==========================================

void rocksdb_account_archive::save_snapshot( const hive::chain::prepare_snapshot_supplement_notification& note )
{
  snapshot->save_snapshot( note );
}

void rocksdb_account_archive::load_snapshot( const hive::chain::load_snapshot_supplement_notification& note )
{
  snapshot->load_snapshot( note );
}

void rocksdb_account_archive::open()
{
  provider->init( destroy_database_on_startup );
}

void rocksdb_account_archive::close()
{
  provider->shutdownDb( destroy_database_on_shutdown );
}

void rocksdb_account_archive::wipe()
{
  provider->wipeDb();
}

} }

HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE( hive::chain::volatile_account_metadata_index )
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE( hive::chain::volatile_account_authority_index )
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE( hive::chain::volatile_account_index )
