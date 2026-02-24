#include <chainbase/chainbase.hpp>

#include <hive/chain/util/type_registrar_definition.hpp>
#include <hive/chain/global_property_object.hpp>
#include <hive/chain/detail/state/global_property_object_multiindex.hpp>

#include <hive/chain/external_storage/rocksdb_account_archive.hpp>
#include <hive/chain/external_storage/utilities.hpp>
#include <hive/chain/external_storage/rocksdb_account_storage_provider.hpp>
#include <hive/chain/external_storage/rocksdb_snapshot.hpp>
#include <hive/chain/external_storage/state_snapshot_provider.hpp>

#include <hive/chain/external_storage/account_metadata_rocksdb_objects.hpp>
#include <hive/chain/external_storage/account_authority_rocksdb_objects.hpp>
#include <hive/chain/external_storage/account_rocksdb_objects.hpp>
#include <hive/chain/external_storage/split_rocksdb_objects.hpp>

// Split object headers for multiindex access
#include <hive/chain/detail/state/recovery_object.hpp>
#include <hive/chain/detail/state/assets_object.hpp>
#include <hive/chain/detail/state/manabars_rc_object.hpp>
#include <hive/chain/detail/state/delayed_votes_object.hpp>
#include <hive/chain/detail/state/tiny_account_object.hpp>

#include <boost/scope_exit.hpp>

#include <limits>

namespace hive { namespace chain {

template<typename SHM_Object_Type, typename RocksDB_Object_Type, typename Slice_Type>
struct rocksdb_storage_writer
{
  static void write_to_storage( const external_storage_provider::ptr& provider, const Slice_Type& key, const SHM_Object_Type& object, ColumnTypes column_type )
  {
    RocksDB_Object_Type _obj( object );

    auto _serialize_buffer = dump( _obj );
    Slice _value( _serialize_buffer.data(), _serialize_buffer.size() );

    provider->save( column_type, key, _value );
  }
};

template<typename SHM_Object_Type, typename RocksDB_Object_Type, typename RocksDB_Object_Type2>
struct rocksdb_writer
{
  static void write_to_storage( const external_storage_provider::ptr& provider, const SHM_Object_Type& object, const std::vector<ColumnTypes>& column_types )
  {
    FC_ASSERT( column_types.size() && "move to external storage" );
    rocksdb_storage_writer<SHM_Object_Type, RocksDB_Object_Type, account_name_slice_t>::write_to_storage( provider, account_name_slice_t( object.get_name().data ), object, column_types[0] );
  }
};

template<>
struct rocksdb_writer<account_metadata_object, rocksdb_account_metadata_object, rocksdb_account_metadata_object>
{
  static void write_to_storage( const external_storage_provider::ptr& provider, const account_metadata_object& object, const std::vector<ColumnTypes>& column_types, const account_name_type& account_name )
  {
    FC_ASSERT( column_types.size() && "move account metadata to external storage" );
    rocksdb_storage_writer<account_metadata_object, rocksdb_account_metadata_object, account_name_slice_t>::write_to_storage( provider, account_name_slice_t( account_name.data ), object, column_types[0] );
  }
};

template<>
struct rocksdb_writer<account_object, rocksdb_account_object, rocksdb_account_object_by_id>
{
  static void write_to_storage( const external_storage_provider::ptr& provider, const account_object& object, const std::vector<ColumnTypes>& column_types )
  {
    FC_ASSERT( column_types.size() == 2 && "move an account to rocksdb storage" );
    rocksdb_storage_writer<account_object, rocksdb_account_object, account_name_slice_t>::write_to_storage( provider, account_name_slice_t( object.get_name().data ), object, column_types[0] );
    rocksdb_storage_writer<account_object, rocksdb_account_object_by_id, uint32_slice_t>::write_to_storage( provider, uint32_slice_t( object.get_id() ), object, column_types[1] );
  }
};

// Split object writers - keyed by account_id
template<typename SHM_Object_Type, typename RocksDB_Object_Type>
struct rocksdb_split_writer
{
  static void write_to_storage( const external_storage_provider::ptr& provider, const SHM_Object_Type& object, ColumnTypes column_type )
  {
    rocksdb_storage_writer<SHM_Object_Type, RocksDB_Object_Type, uint32_slice_t>::write_to_storage( provider, uint32_slice_t( account_id_type( account_object::id_type( object.get_id().get_value() ) ) ), object, column_type );
  }
};

template<typename Slice_Type, typename Pinnable_Type = PinnableSlice>
struct rocksdb_storage_reader
{
  static bool read_from_storage( const external_storage_provider::ptr& provider, const Slice_Type& key, ColumnTypes column_type, Pinnable_Type& buffer )
  {
    return provider->read( column_type, key, buffer );
  }
};

template<typename SHM_Object_Type, typename Key_Type, typename Return_Type>
struct rocksdb_reader
{
};

template<typename Return_Type>
struct rocksdb_reader<account_metadata_object, account_name_type, Return_Type>
{
  static Return_Type read( chainbase::database& db, const external_storage_provider::ptr& provider, const account_name_type& key, const std::vector<ColumnTypes>& column_types )
  {
    PinnableSlice _buffer;

    FC_ASSERT( column_types.size() && "read account metadata from rocksdb storage" );
    if( !rocksdb_storage_reader<account_name_slice_t>::read_from_storage( provider, account_name_slice_t( key.data ), column_types[0], _buffer ) )
      return nullptr;

    rocksdb_account_metadata_object _obj;

    load( _obj, _buffer.data(), _buffer.size() );

    return _obj.build<Return_Type>( db );
  }
};

template<typename Return_Type>
struct rocksdb_reader<account_authority_object, account_name_type, Return_Type>
{
  static Return_Type read( chainbase::database& db, const external_storage_provider::ptr& provider, const account_name_type& key, const std::vector<ColumnTypes>& column_types )
  {
    PinnableSlice _buffer;

    FC_ASSERT( column_types.size() && "read account authority from rocksdb storage" );
    if( !rocksdb_storage_reader<account_name_slice_t>::read_from_storage( provider, account_name_slice_t( key.data ), column_types[0], _buffer ) )
      return nullptr;

    rocksdb_account_authority_object _obj;

    load( _obj, _buffer.data(), _buffer.size() );

    return _obj.build<Return_Type>( db );
  }
};

template<typename Return_Type>
struct rocksdb_reader<account_object, account_name_type, Return_Type>
{
  static Return_Type read( chainbase::database& db, const external_storage_provider::ptr& provider, const account_name_type& key, const std::vector<ColumnTypes>& column_types )
  {
    PinnableSlice _buffer;

    FC_ASSERT( column_types.size() && "read account from rocksdb storage" );
    if( !rocksdb_storage_reader<account_name_slice_t>::read_from_storage( provider, account_name_slice_t( key.data ), column_types[0], _buffer ) )
      return nullptr;

    rocksdb_account_object _obj;

    load( _obj, _buffer.data(), _buffer.size() );

    auto result = _obj.build<Return_Type>( db );

    // Also restore the split objects associated with this account
    // Note: check if each split object already exists in chainbase before creating.
    // Objects created with create_no_undo survive pop_block() during fork switches,
    // so we must avoid duplicate creation on subsequent restoration attempts.
    if constexpr ( std::is_same_v<Return_Type, const account_object*> )
    {
      const account_id_type account_id = result->get_id();
      const auto aid = account_id.get_value();

      // Recovery object
      if( !db.find< recovery_object, by_id >( recovery_object::id_type( aid ) ) )
      {
        PinnableSlice split_buffer;
        if( rocksdb_storage_reader<uint32_slice_t>::read_from_storage( provider, uint32_slice_t( account_id ), ColumnTypes::RECOVERY, split_buffer ) )
        {
          rocksdb_recovery_object split_obj;
          load( split_obj, split_buffer.data(), split_buffer.size() );
          split_obj.build<const recovery_object*>( db );
        }
      }

      // Assets object
      if( !db.find< assets_object, by_id >( assets_object::id_type( aid ) ) )
      {
        PinnableSlice split_buffer;
        if( rocksdb_storage_reader<uint32_slice_t>::read_from_storage( provider, uint32_slice_t( account_id ), ColumnTypes::ASSETS, split_buffer ) )
        {
          rocksdb_assets_object split_obj;
          load( split_obj, split_buffer.data(), split_buffer.size() );
          split_obj.build<const assets_object*>( db );
        }
      }

      // Manabars_rc object
      if( !db.find< manabars_rc_object, by_id >( manabars_rc_object::id_type( aid ) ) )
      {
        PinnableSlice split_buffer;
        if( rocksdb_storage_reader<uint32_slice_t>::read_from_storage( provider, uint32_slice_t( account_id ), ColumnTypes::MANABARS_RC, split_buffer ) )
        {
          rocksdb_manabars_rc_object split_obj;
          load( split_obj, split_buffer.data(), split_buffer.size() );
          split_obj.build<const manabars_rc_object*>( db );
        }
      }

      // Delayed_votes object
      if( !db.find< delayed_votes_object, by_id >( delayed_votes_object::id_type( aid ) ) )
      {
        PinnableSlice split_buffer;
        if( rocksdb_storage_reader<uint32_slice_t>::read_from_storage( provider, uint32_slice_t( account_id ), ColumnTypes::DELAYED_VOTES, split_buffer ) )
        {
          rocksdb_delayed_votes_object split_obj;
          load( split_obj, split_buffer.data(), split_buffer.size() );
          split_obj.build<const delayed_votes_object*>( db );
        }
      }
    }

    return result;
  }
};

template<typename Return_Type>
struct rocksdb_reader<account_object, account_id_type, Return_Type>
{
  static Return_Type read( chainbase::database& db, const external_storage_provider::ptr& provider, const account_id_type& key, const std::vector<ColumnTypes>& column_types )
  {
    account_name_type _name;

    FC_ASSERT( column_types.size() == 2 && "read an account from rocksdb storage" );

    {
      PinnableSlice _buffer;

      if( !rocksdb_storage_reader<uint32_slice_t>::read_from_storage( provider, uint32_slice_t( key ), column_types[0], _buffer ) )
        return nullptr;

      rocksdb_account_object_by_id _obj;
      load( _obj, _buffer.data(), _buffer.size() );
      _name = _obj.name;
    }

    return rocksdb_reader<account_object, account_name_type, Return_Type>::read( db, provider, _name, { column_types[1] } );
  }
};

// Generic split object reader - keyed by account_id
template<typename SHM_Object_Type, typename RocksDB_Object_Type, typename Return_Type>
struct rocksdb_split_reader
{
  static Return_Type read( chainbase::database& db, const external_storage_provider::ptr& provider, const account_id_type& account_id, ColumnTypes column_type )
  {
    PinnableSlice _buffer;

    if( !rocksdb_storage_reader<uint32_slice_t>::read_from_storage( provider, uint32_slice_t( account_id ), column_type, _buffer ) )
      return nullptr;

    RocksDB_Object_Type _obj;
    load( _obj, _buffer.data(), _buffer.size() );

    return _obj.template build<Return_Type>( db );
  }
};

template<typename Key_Type, typename SHM_Object_Type>
struct message
{
};

template<typename Key_Type>
struct message<Key_Type, account_metadata_object>
{
  static void check( bool is_required, const Key_Type& key )
  {
    FC_ASSERT( !is_required && "account metadata", "Account metadata with key `${key}` not found", ("key", key) );
  }
};

template<typename Key_Type>
struct message<Key_Type, account_authority_object>
{
  static void check( bool is_required, const Key_Type& key )
  {
    FC_ASSERT( !is_required && "account authority", "Account authority with key `${key}` not found", ("key", key) );
  }
};

template<typename Key_Type>
struct message<Key_Type, account_object>
{
  static void check( bool is_required, const Key_Type& key )
  {
    FC_ASSERT( !is_required && "account", "Account with key `${key}` not found", ("key", key) );
  }
};

rocksdb_account_archive::rocksdb_account_archive( database& db, const bfs::path& blockchain_storage_path,
  const bfs::path& storage_path, uint32_t retention_blocks, appbase::application& app )
  : retention_blocks( retention_blocks ), db( db )
{
  provider = std::make_shared<rocksdb_account_storage_provider>( blockchain_storage_path, storage_path, app );
  snapshot = std::make_shared<rocksdb_snapshot>( "Accounts RocksDB", "accounts_rocksdb_data", db, storage_path, provider );
}

rocksdb_account_archive::~rocksdb_account_archive()
{
  close();
}

template<typename SHM_Index_Type, typename SHM_Object_Type,
        typename RocksDB_Object_Type,
        typename RocksDB_Object_Type2 = RocksDB_Object_Type>
bool rocksdb_account_archive::on_irreversible_block_impl( uint32_t block_num, const std::vector<ColumnTypes>& column_types )
{
  const auto& _idx = db.get_index<SHM_Index_Type, by_block>();

  auto time_start = std::chrono::high_resolution_clock::now();

  auto _itr = _idx.begin();

  bool _do_flush = false;
  uint32_t _cnt = 0;

  /*
    Regarding: `itr->get_last_access_block() < block_num`
    Here must be `<` not `<=` because `get_last_access_block` is a number of last block, but `block_num` is the current block.
  */
  while( _itr != _idx.end() && _itr->get_last_access_block() + retention_blocks < block_num )
  {
    const auto& _current = *_itr;
    ++_itr;

    if( _current.changed() )
    {
      ++_cnt;

      rocksdb_writer<SHM_Object_Type, RocksDB_Object_Type, RocksDB_Object_Type2>::write_to_storage( provider, _current, column_types );

      if( !_do_flush )
        _do_flush = true;
    }

    db.remove_no_undo( _current );
  }

  accounts_stats::stats.item_moved_to_storage.time_ns += std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
  accounts_stats::stats.item_moved_to_storage.count += _cnt;

  return _do_flush;
}

// Specialized implementation for account_object which skips accounts with pending governance votes
// Also archives the associated split objects (recovery, assets, manabars_rc, delayed_votes)
bool rocksdb_account_archive::on_irreversible_block_impl_account( uint32_t block_num, const std::vector<ColumnTypes>& column_types )
{
  const auto& _idx = db.get_index<account_index, by_block>();

  auto time_start = std::chrono::high_resolution_clock::now();

  auto _itr = _idx.begin();

  bool _do_flush = false;
  uint32_t _cnt = 0;

  /*
    Regarding: `itr->get_last_access_block() < block_num`
    Here must be `<` not `<=` because `get_last_access_block` is a number of last block, but `block_num` is the current block.
  */
  while( _itr != _idx.end() && _itr->get_last_access_block() + retention_blocks < block_num )
  {
    const auto& _current = *_itr;
    ++_itr;

    const account_id_type account_id = _current.get_id();
    const auto aid = account_id.get_value();

    // Use tiny_account_object to check archival guards (it is always in chainbase)
    const auto& tiny_idx = db.get_index< tiny_account_index, by_name >();
    auto tiny_it = tiny_idx.find( _current.get_name() );

    // Skip accounts with pending governance votes - they need to stay in chainbase
    if( tiny_it != tiny_idx.end() && tiny_it->get_governance_vote_expiration_ts() != fc::time_point_sec::maximum() )
      continue;

    // Skip accounts with active power-down - they need to stay in chainbase
    if( tiny_it != tiny_idx.end() && tiny_it->get_next_vesting_withdrawal() != fc::time_point_sec::maximum() )
      continue;

    // Find all split objects before removing anything - use by_id index
    const auto* recovery_ptr = db.find< recovery_object, by_id >( recovery_object::id_type( aid ) );
    const auto* assets_ptr = db.find< assets_object, by_id >( assets_object::id_type( aid ) );
    const auto* manabars_ptr = db.find< manabars_rc_object, by_id >( manabars_rc_object::id_type( aid ) );
    const auto* delayed_votes_ptr = db.find< delayed_votes_object, by_id >( delayed_votes_object::id_type( aid ) );

    // Archive changed account data to RocksDB
    if( _current.changed() )
    {
      ++_cnt;

      rocksdb_writer<account_object, rocksdb_account_object, rocksdb_account_object_by_id>::write_to_storage( provider, _current, column_types );

      if( !_do_flush )
        _do_flush = true;
    }

    // Always archive split objects to RocksDB (even if account hasn't changed)
    // This ensures split object data is preserved when account is archived
    if( recovery_ptr )
      rocksdb_split_writer<recovery_object, rocksdb_recovery_object>::write_to_storage( provider, *recovery_ptr, ColumnTypes::RECOVERY );

    if( assets_ptr )
      rocksdb_split_writer<assets_object, rocksdb_assets_object>::write_to_storage( provider, *assets_ptr, ColumnTypes::ASSETS );

    if( manabars_ptr )
      rocksdb_split_writer<manabars_rc_object, rocksdb_manabars_rc_object>::write_to_storage( provider, *manabars_ptr, ColumnTypes::MANABARS_RC );

    if( delayed_votes_ptr )
      rocksdb_split_writer<delayed_votes_object, rocksdb_delayed_votes_object>::write_to_storage( provider, *delayed_votes_ptr, ColumnTypes::DELAYED_VOTES );

    if( !_do_flush && (recovery_ptr || assets_ptr || manabars_ptr || delayed_votes_ptr) )
      _do_flush = true;

    // Remove the account from chainbase
    db.remove_no_undo( _current );

    // Also remove the split objects from chainbase
    // NOTE: tiny_account_object is NOT removed - it stays permanently in chainbase
    if( recovery_ptr )
      db.remove_no_undo( *recovery_ptr );

    if( assets_ptr )
      db.remove_no_undo( *assets_ptr );

    if( manabars_ptr )
      db.remove_no_undo( *manabars_ptr );

    if( delayed_votes_ptr )
      db.remove_no_undo( *delayed_votes_ptr );
  }

  accounts_stats::stats.item_moved_to_storage.time_ns += std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
  accounts_stats::stats.item_moved_to_storage.count += _cnt;

  return _do_flush;
}

// Specialized implementation for account_metadata_object which needs account name lookup
bool rocksdb_account_archive::on_irreversible_block_impl_metadata( uint32_t block_num, const std::vector<ColumnTypes>& column_types )
{
  const auto& _idx = db.get_index<account_metadata_index, by_block>();

  auto time_start = std::chrono::high_resolution_clock::now();

  auto _itr = _idx.begin();

  bool _do_flush = false;
  uint32_t _cnt = 0;

  while( _itr != _idx.end() && _itr->get_last_access_block() + retention_blocks < block_num )
  {
    const auto& _current = *_itr;
    ++_itr;

    if( _current.changed() )
    {
      ++_cnt;

      // For account_metadata_object, we need to look up the account name from the account_id
      const auto* account_ptr = db.find< account_object, by_id >( _current.account );
      if( account_ptr )
      {
        rocksdb_writer<account_metadata_object, rocksdb_account_metadata_object, rocksdb_account_metadata_object>::write_to_storage( provider, _current, column_types, account_ptr->get_name() );
      }
      // If account not found in chainbase, it may already be in RocksDB - skip writing metadata without account

      if( !_do_flush )
        _do_flush = true;
    }

    db.remove_no_undo( _current );
  }

  accounts_stats::stats.item_moved_to_storage.time_ns += std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
  accounts_stats::stats.item_moved_to_storage.count += _cnt;

  return _do_flush;
}

uint32_t rocksdb_account_archive::compute_next_archival_check() const
{
  uint32_t min_block = std::numeric_limits<uint32_t>::max();

  {
    const auto& idx = db.get_index<account_metadata_index, by_block>();
    if( !idx.empty() )
    {
      uint32_t candidate = idx.begin()->get_last_access_block() + retention_blocks + 1;
      if( candidate < min_block )
        min_block = candidate;
    }
  }

  {
    const auto& idx = db.get_index<account_authority_index, by_block>();
    if( !idx.empty() )
    {
      uint32_t candidate = idx.begin()->get_last_access_block() + retention_blocks + 1;
      if( candidate < min_block )
        min_block = candidate;
    }
  }

  {
    const auto& idx = db.get_index<account_index, by_block>();
    if( !idx.empty() )
    {
      uint32_t candidate = idx.begin()->get_last_access_block() + retention_blocks + 1;
      if( candidate < min_block )
        min_block = candidate;
    }
  }

  return min_block;
}

void rocksdb_account_archive::on_irreversible_block( uint32_t block_num )
{
  provider->update_cached_lib( block_num );

  // Skip archival scan if no objects can possibly need archiving yet
  if( block_num < _next_archival_check_block )
    return;

  if( objects_limit )
  {
    size_t _nr_elements = 0;
    _nr_elements += db.get_index<account_metadata_index, by_block>().size();
    _nr_elements += db.get_index<account_authority_index, by_block>().size();
    _nr_elements += db.get_index<account_index, by_block>().size();

    if( _nr_elements < objects_limit )
      return;
  }

  // Use specialized method for account_metadata_object (needs account name lookup)
  bool _do_flush_meta = on_irreversible_block_impl_metadata( block_num, { ColumnTypes::ACCOUNT_METADATA } );

  bool _do_flush_authority = on_irreversible_block_impl<
                          account_authority_index, account_authority_object, rocksdb_account_authority_object>
                          ( block_num, { ColumnTypes::ACCOUNT_AUTHORITY } );

  // Use specialized method for account_object (skips accounts with pending governance votes)
  bool _do_flush_account = on_irreversible_block_impl_account( block_num, { ColumnTypes::ACCOUNT, ColumnTypes::ACCOUNT_BY_ID } );

  if( _do_flush_meta || _do_flush_authority || _do_flush_account )
  {
    auto time_start = std::chrono::high_resolution_clock::now();

    provider->persist_cached_lib();
    provider->flushDb();

    accounts_stats::stats.item_flush_to_storage.time_ns += std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
    ++accounts_stats::stats.item_flush_to_storage.count;

    /*
      Do compact only when there is no limit on objects, i.e we are in live mode.
      In testnet compaction is not needed it only slows down tests.
    */
#ifndef IS_TEST_NET
    if( objects_limit == 0 )
    {
      ++compaction_frequency_counter;
      if( compaction_frequency_counter >= compaction_frequency )
      {
        auto _time_start_for_compaction = std::chrono::high_resolution_clock::now();

        provider->compaction();
        compaction_frequency_counter = 0;

        accounts_stats::stats.compact_storage.time_ns += std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - _time_start_for_compaction ).count();
        ++accounts_stats::stats.compact_storage.count;
      }
    }
#endif

  }

  _next_archival_check_block = compute_next_archival_check();
}

template<typename Key_Type, typename SHM_Object_Type, typename SHM_Object_Sub_Index, typename Return_Type>
Return_Type rocksdb_account_archive::get_object( const Key_Type& key, const std::vector<ColumnTypes>& column_types, bool is_required ) const
{
  const auto* _found = db.find<SHM_Object_Type, SHM_Object_Sub_Index>( key );
  if( _found )
  {
    if constexpr ( std::is_same_v<Return_Type, const SHM_Object_Type*> )
      return _found;
    else
      return Return_Type( _found );
  }
  else
  {
    Return_Type _external_found = rocksdb_reader<SHM_Object_Type, Key_Type, Return_Type>::read( db, provider, key, column_types );
    if( _external_found )
    {
      // Set last_access_block on restored object and its split objects to prevent immediate re-archival
      if constexpr ( std::is_same_v<Return_Type, const SHM_Object_Type*> )
      {
        static_cast<chainbase::database&>(db).modify( *_external_found, [&]( SHM_Object_Type& o )
        {
          o.set_last_access_block( db.head_block_num() );
        });
      }
    }
    else
    {
      message<Key_Type, SHM_Object_Type>::check( is_required, key );
    }
    return _external_found;
  }
}

//==========================================account_metadata_object==========================================
void rocksdb_account_archive::create_object( const account_metadata_object& obj )
{
  auto time_start = std::chrono::high_resolution_clock::now();

  // Get head block number safely (returns 0 during genesis when dynamic_global_property_object doesn't exist yet)
  const auto* dgpo = db.find< dynamic_global_property_object >();
  uint32_t head_block = dgpo ? dgpo->head_block_number : 0;

  // Set last_access_block to mark the object as changed, so it will be written to RocksDB when archived
  // Use static_cast to bypass accounts_handler and avoid infinite recursion
  static_cast<chainbase::database&>(db).modify( obj, [head_block]( account_metadata_object& o )
  {
    o.set_last_access_block( head_block );
  } );

  accounts_stats::stats.account_metadata_created.time_ns += std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
  ++accounts_stats::stats.account_metadata_created.count;
}

const account_metadata_object* rocksdb_account_archive::get_account_metadata( const account_name_type& account_name, bool account_metadata_is_required ) const
{
  auto time_start = std::chrono::high_resolution_clock::now();

  BOOST_SCOPE_EXIT_ALL(&)
  {
    accounts_stats::stats.account_metadata_accessed_by_name.time_ns += std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
    ++accounts_stats::stats.account_metadata_accessed_by_name.count;
  };

  // In split architecture, we need to first find the account, then get metadata by account_id
  const auto* account_ptr = db.find< account_object, by_name >( account_name );
  if( !account_ptr )
  {
    // Try to find in RocksDB
    auto _external_found = rocksdb_reader<account_object, account_name_type, const account_object*>::read( db, provider, account_name, { ColumnTypes::ACCOUNT } );
    if( !_external_found )
    {
      if( account_metadata_is_required )
        FC_ASSERT( !"ACCOUNT_DOES_NOT_EXIST", "Account metadata for `${name}` not found - account does not exist", ("name", account_name) );
      return nullptr;
    }
    account_ptr = _external_found;
  }

  const auto* _found = db.find<account_metadata_object, by_account>( account_ptr->get_id() );
  if( _found )
    return _found;

  // Try to find in RocksDB
  auto _external_found = rocksdb_reader<account_metadata_object, account_name_type, const account_metadata_object*>::read( db, provider, account_name, { ColumnTypes::ACCOUNT_METADATA } );
  if( _external_found )
  {
    // Set last_access_block to prevent immediate re-archival
    static_cast<chainbase::database&>(db).modify( *_external_found, [&]( account_metadata_object& o )
    {
      o.set_last_access_block( db.head_block_num() );
    });
    return _external_found;
  }

  if( account_metadata_is_required )
    FC_ASSERT( !"METADATA_OBJECT_MISSING", "Account metadata for `${name}` not found - metadata object missing", ("name", account_name) );

  return nullptr;
}

account_metadata rocksdb_account_archive::get_volatile_account_metadata( const account_name_type& account_name, bool account_metadata_is_required ) const
{
  auto time_start = std::chrono::high_resolution_clock::now();

  BOOST_SCOPE_EXIT_ALL(&)
  {
    accounts_stats::stats.account_metadata_accessed_by_name.time_ns += std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
    ++accounts_stats::stats.account_metadata_accessed_by_name.count;
  };

  const auto* _found = get_account_metadata( account_name, account_metadata_is_required );
  if( _found )
    return account_metadata( _found );
  return nullptr;
}

void rocksdb_account_archive::on_object_modified( const account_metadata_object& obj )
{
  ++accounts_stats::stats.account_metadata_modified.count;
}
//==========================================account_metadata_object==========================================

//==========================================account_authority_object==========================================
void rocksdb_account_archive::create_object( const account_authority_object& obj )
{
  auto time_start = std::chrono::high_resolution_clock::now();

  // Get head block number safely (returns 0 during genesis when dynamic_global_property_object doesn't exist yet)
  const auto* dgpo = db.find< dynamic_global_property_object >();
  uint32_t head_block = dgpo ? dgpo->head_block_number : 0;

  // Set last_access_block to mark the object as changed, so it will be written to RocksDB when archived
  // Use static_cast to bypass accounts_handler and avoid infinite recursion
  static_cast<chainbase::database&>(db).modify( obj, [head_block]( account_authority_object& o )
  {
    o.set_last_access_block( head_block );
  } );

  accounts_stats::stats.account_authority_created.time_ns += std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
  ++accounts_stats::stats.account_authority_created.count;
}

const account_authority_object* rocksdb_account_archive::get_account_authority( const account_name_type& account_name, bool account_authority_is_required ) const
{
  auto time_start = std::chrono::high_resolution_clock::now();

  BOOST_SCOPE_EXIT_ALL(&)
  {
    accounts_stats::stats.account_authority_accessed_by_name.time_ns += std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
    ++accounts_stats::stats.account_authority_accessed_by_name.count;
  };

  // In split architecture, by_account uses composite key with account_name as first element
  const auto* _found = db.find< account_authority_object, by_account >( account_name );
  if( _found )
    return _found;

  // Try to find in RocksDB
  auto _external_found = rocksdb_reader<account_authority_object, account_name_type, const account_authority_object*>::read( db, provider, account_name, { ColumnTypes::ACCOUNT_AUTHORITY } );
  if( _external_found )
  {
    // Set last_access_block to prevent immediate re-archival
    static_cast<chainbase::database&>(db).modify( *_external_found, [&]( account_authority_object& o )
    {
      o.set_last_access_block( db.head_block_num() );
    });
    return _external_found;
  }

  if( account_authority_is_required )
    FC_ASSERT( !"AUTHORITY_NOT_FOUND", "Account authority for `${name}` not found in authority index", ("name", account_name) );

  return nullptr;
}

account_authority rocksdb_account_archive::get_volatile_account_authority( const account_name_type& account_name, bool account_authority_is_required ) const
{
  auto time_start = std::chrono::high_resolution_clock::now();

  BOOST_SCOPE_EXIT_ALL(&)
  {
    accounts_stats::stats.account_authority_accessed_by_name.time_ns += std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
    ++accounts_stats::stats.account_authority_accessed_by_name.count;
  };

  const auto* _found = get_account_authority( account_name, account_authority_is_required );
  if( _found )
    return account_authority( _found );
  return nullptr;
}

void rocksdb_account_archive::on_object_modified( const account_authority_object& obj )
{
  ++accounts_stats::stats.account_authority_modified.count;
}
//==========================================account_authority_object==========================================

//==========================================account_object==========================================
void rocksdb_account_archive::create_object( const account_object& obj )
{
  auto time_start = std::chrono::high_resolution_clock::now();

  // Get head block number safely (returns 0 during genesis when dynamic_global_property_object doesn't exist yet)
  const auto* dgpo = db.find< dynamic_global_property_object >();
  uint32_t head_block = dgpo ? dgpo->head_block_number : 0;

  // Set last_access_block to mark the object as changed, so it will be written to RocksDB when archived
  // Use static_cast to bypass accounts_handler and avoid infinite recursion
  static_cast<chainbase::database&>(db).modify( obj, [head_block]( account_object& o )
  {
    o.set_last_access_block( head_block );
  } );

  // Create the tiny_account_object that stays in chainbase permanently
  const auto aid = obj.get_id().get_value();
  const auto* assets_ptr = db.find< assets_object, by_id >( assets_object::id_type( aid ) );
  const auto* dvotes_ptr = db.find< delayed_votes_object, by_id >( delayed_votes_object::id_type( aid ) );
  if( assets_ptr && dvotes_ptr )
  {
    db.create< tiny_account_object >( obj, *assets_ptr, *dvotes_ptr );
  }

  accounts_stats::stats.account_created.time_ns += std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
  ++accounts_stats::stats.account_created.count;
}

const account_object* rocksdb_account_archive::get_account( const account_name_type& account_name, bool account_is_required ) const
{
  auto time_start = std::chrono::high_resolution_clock::now();

  BOOST_SCOPE_EXIT_ALL(&)
  {
    accounts_stats::stats.account_accessed_by_name.time_ns += std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
    ++accounts_stats::stats.account_accessed_by_name.count;
  };

  const auto* _found = db.find< account_object, by_name >( account_name );
  if( _found )
    return _found;

  // Try to find in RocksDB
  auto _external_found = rocksdb_reader<account_object, account_name_type, const account_object*>::read( db, provider, account_name, { ColumnTypes::ACCOUNT } );
  if( _external_found )
  {
    // Set last_access_block to prevent immediate re-archival
    static_cast<chainbase::database&>(db).modify( *_external_found, [&]( account_object& o )
    {
      o.set_last_access_block( db.head_block_num() );
    });
    return _external_found;
  }

  if( account_is_required )
    FC_ASSERT( !"ACCOUNT_NOT_FOUND_BY_NAME", "Account `${name}` not found by name", ("name", account_name) );

  return nullptr;
}

const account_object* rocksdb_account_archive::get_account( const account_id_type& account_id, bool account_is_required ) const
{
  auto time_start = std::chrono::high_resolution_clock::now();

  BOOST_SCOPE_EXIT_ALL(&)
  {
    accounts_stats::stats.account_accessed_by_id.time_ns += std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
    ++accounts_stats::stats.account_accessed_by_id.count;
  };

  const auto* _found = db.find< account_object, by_id >( account_id );
  if( _found )
    return _found;

  // Try to find in RocksDB
  auto _external_found = rocksdb_reader<account_object, account_id_type, const account_object*>::read( db, provider, account_id, { ColumnTypes::ACCOUNT_BY_ID, ColumnTypes::ACCOUNT } );
  if( _external_found )
  {
    // Set last_access_block to prevent immediate re-archival
    static_cast<chainbase::database&>(db).modify( *_external_found, [&]( account_object& o )
    {
      o.set_last_access_block( db.head_block_num() );
    });
    return _external_found;
  }

  if( account_is_required )
    FC_ASSERT( !"ACCOUNT_NOT_FOUND_BY_ID", "Account with id `${id}` not found by id", ("id", account_id) );

  return nullptr;
}

account rocksdb_account_archive::get_volatile_account( const account_name_type& account_name, bool account_is_required ) const
{
  auto time_start = std::chrono::high_resolution_clock::now();

  BOOST_SCOPE_EXIT_ALL(&)
  {
    accounts_stats::stats.account_accessed_by_name.time_ns += std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
    ++accounts_stats::stats.account_accessed_by_name.count;
  };

  const auto* _found = get_account( account_name, account_is_required );
  if( _found )
    return account( _found );
  return nullptr;
}

account rocksdb_account_archive::get_volatile_account( const account_id_type& account_id, bool account_is_required ) const
{
  auto time_start = std::chrono::high_resolution_clock::now();

  BOOST_SCOPE_EXIT_ALL(&)
  {
    accounts_stats::stats.account_accessed_by_id.time_ns += std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
    ++accounts_stats::stats.account_accessed_by_id.count;
  };

  const auto* _found = get_account( account_id, account_is_required );
  if( _found )
    return account( _found );
  return nullptr;
}

void rocksdb_account_archive::on_object_modified( const account_object& obj )
{
  // Sync tiny_account_object with account_object changes (proxy, governance_vote_expiration_ts)
  const auto& tiny_idx = db.get_index< tiny_account_index, by_name >();
  auto tiny_it = tiny_idx.find( obj.get_name() );
  if( tiny_it != tiny_idx.end() )
  {
    static_cast<chainbase::database&>(db).modify( *tiny_it, [&]( tiny_account_object& t )
    {
      t.modify_from_account( obj );
    } );
  }

  ++accounts_stats::stats.account_modified.count;
}
//==========================================account_object==========================================

//==========================================split objects==========================================

const assets_object* rocksdb_account_archive::get_asset_account( const account_id_type& account_id, bool is_required ) const
{
  const auto aid = account_id.get_value();
  const auto* _found = db.find< assets_object, by_id >( assets_object::id_type( aid ) );
  if( _found )
    return _found;

  // Try to restore from RocksDB (get_account restores all split objects)
  if( get_account( account_id, false /*account_is_required*/ ) )
  {
    _found = db.find< assets_object, by_id >( assets_object::id_type( aid ) );
    if( _found )
      return _found;
  }

  if( is_required )
    FC_ASSERT( !"ASSETS_NOT_FOUND", "Assets object for account with id `${id}` not found", ("id", account_id) );

  return nullptr;
}

const recovery_object* rocksdb_account_archive::get_recovery_account( const account_id_type& account_id, bool is_required ) const
{
  const auto aid = account_id.get_value();
  const auto* _found = db.find< recovery_object, by_id >( recovery_object::id_type( aid ) );
  if( _found )
    return _found;

  // Try to restore from RocksDB (get_account restores all split objects)
  if( get_account( account_id, false /*account_is_required*/ ) )
  {
    _found = db.find< recovery_object, by_id >( recovery_object::id_type( aid ) );
    if( _found )
      return _found;
  }

  if( is_required )
    FC_ASSERT( !"RECOVERY_NOT_FOUND", "Recovery object for account with id `${id}` not found", ("id", account_id) );

  return nullptr;
}

const manabars_rc_object* rocksdb_account_archive::get_manabars_rc_account( const account_id_type& account_id, bool is_required ) const
{
  const auto aid = account_id.get_value();
  const auto* _found = db.find< manabars_rc_object, by_id >( manabars_rc_object::id_type( aid ) );
  if( _found )
    return _found;

  // Try to restore from RocksDB (get_account restores all split objects)
  if( get_account( account_id, false /*account_is_required*/ ) )
  {
    _found = db.find< manabars_rc_object, by_id >( manabars_rc_object::id_type( aid ) );
    if( _found )
      return _found;
  }

  if( is_required )
    FC_ASSERT( !"MANABARS_RC_NOT_FOUND", "Manabars RC object for account with id `${id}` not found", ("id", account_id) );

  return nullptr;
}

const delayed_votes_object* rocksdb_account_archive::get_delayed_votes_account( const account_id_type& account_id, bool is_required ) const
{
  const auto aid = account_id.get_value();
  const auto* _found = db.find< delayed_votes_object, by_id >( delayed_votes_object::id_type( aid ) );
  if( _found )
    return _found;

  // Try to restore from RocksDB (get_account restores all split objects)
  if( get_account( account_id, false /*account_is_required*/ ) )
  {
    _found = db.find< delayed_votes_object, by_id >( delayed_votes_object::id_type( aid ) );
    if( _found )
      return _found;
  }

  if( is_required )
    FC_ASSERT( !"DELAYED_VOTES_NOT_FOUND", "Delayed votes object for account with id `${id}` not found", ("id", account_id) );

  return nullptr;
}

//==========================================split objects==========================================

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
  provider->openDb( db.get_last_irreversible_block_num() );
}

void rocksdb_account_archive::close()
{
  if( provider->getStorage() )
    provider->persist_cached_lib();
  provider->shutdownDb();
}

void rocksdb_account_archive::wipe()
{
  provider->wipeDb();
}

void rocksdb_account_archive::remove_objects_limit()
{
  objects_limit = 0;
}

} }
