#include <chainbase/chainbase.hpp>

#include <hive/chain/util/type_registrar_definition.hpp>

#include <hive/chain/external_storage/rocksdb_account_archive.hpp>
#include <hive/chain/external_storage/utilities.hpp>
#include <hive/chain/external_storage/rocksdb_account_storage_provider.hpp>
#include <hive/chain/external_storage/rocksdb_snapshot.hpp>
#include <hive/chain/external_storage/state_snapshot_provider.hpp>

#include <hive/chain/external_storage/account_metadata_rocksdb_objects.hpp>
#include <hive/chain/external_storage/account_authority_rocksdb_objects.hpp>
#include <hive/chain/external_storage/account_rocksdb_objects.hpp>
#include <hive/chain/external_storage/allocator_helper.hpp>

#include <boost/scope_exit.hpp>

namespace hive { namespace chain {

//#define DBG_INFO
//#define DBG_MOVE_INFO
//#define DBG_MOVE_DETAILS_INFO

template<typename Object_Type, typename RocksDB_Object_Type, typename Slice_Type>
struct transporter_impl
{
  static void move_to_external_storage( const external_storage_reader_writer::ptr& provider, const Slice_Type& key, const Object_Type& object, ColumnTypes column_type )
  {
    RocksDB_Object_Type _obj( object );

    auto _serialize_buffer = dump( _obj );
    Slice _value( _serialize_buffer.data(), _serialize_buffer.size() );

    provider->save( column_type, key, _value );
  }
};

template<typename SHM_Object_Type, typename RocksDB_Object_Type, typename RocksDB_Object_Type2>
struct transporter
{
  static void move_to_external_storage( const external_storage_reader_writer::ptr& provider, const SHM_Object_Type& object, const std::vector<ColumnTypes>& column_types )
  {
    FC_ASSERT( column_types.size() && "move to external storage" );
    transporter_impl<SHM_Object_Type, RocksDB_Object_Type, account_name_slice_t>::move_to_external_storage( provider, account_name_slice_t( object.get_name().data ), object, column_types[0] );
  }
};

template<>
struct transporter<account_object, rocksdb_account_object, rocksdb_account_object_by_id>
{
  static void move_to_external_storage( const external_storage_reader_writer::ptr& provider, const account_object& object, const std::vector<ColumnTypes>& column_types )
  {
    FC_ASSERT( column_types.size() == 2 && "move an account to rocksdb storage" );
    transporter_impl<account_object, rocksdb_account_object, account_name_slice_t>::move_to_external_storage( provider, account_name_slice_t( object.get_name().data ), object, column_types[0] );
    transporter_impl<account_object, rocksdb_account_object_by_id, uint32_slice_t>::move_to_external_storage( provider, uint32_slice_t( object.get_account_id() ), object, column_types[1] );
  }
};

struct rocksdb_reader_helper
{
  template<typename Slice_Type, typename Key_Type>
  static bool read( const external_storage_reader_writer::ptr& provider, const Key_Type& key, ColumnTypes column_type, PinnableSlice& buffer )
  {
    Slice_Type _key( key );

    return provider->read( column_type, _key, buffer );
  }

};

template<typename SHM_Object_Type, typename Key_Type>
struct rocksdb_reader
{
};

template<>
struct rocksdb_reader<account_metadata_object, account_name_type>
{
  static const account_metadata_object* read( chainbase::database& db, const external_storage_reader_writer::ptr& provider, const account_name_type& key, const std::vector<ColumnTypes>& column_types )
  {
    PinnableSlice _buffer;

    FC_ASSERT( column_types.size() && "read account metadata from rocksdb storage" );
    if( !rocksdb_reader_helper::read<account_name_slice_t, account_name_type::Storage>( provider, key.data, column_types[0], _buffer ) )
      return nullptr;

    rocksdb_account_metadata_object _obj;

    load( _obj, _buffer.data(), _buffer.size() );

    return _obj.build( db );
  }
};

template<>
struct rocksdb_reader<account_authority_object, account_name_type>
{
  static const account_authority_object* read( chainbase::database& db, const external_storage_reader_writer::ptr& provider, const account_name_type& key, const std::vector<ColumnTypes>& column_types )
  {
    PinnableSlice _buffer;

    FC_ASSERT( column_types.size() && "read account authority from rocksdb storage" );
    if( !rocksdb_reader_helper::read<account_name_slice_t, account_name_type::Storage>( provider, key.data, column_types[0], _buffer ) )
      return nullptr;

    rocksdb_account_authority_object _obj;

    load( _obj, _buffer.data(), _buffer.size() );

    return _obj.build( db );
  }
};

template<>
struct rocksdb_reader<account_object, account_name_type>
{
  static const account_object* read( chainbase::database& db, const external_storage_reader_writer::ptr& provider, const account_name_type& key, const std::vector<ColumnTypes>& column_types )
  {
    PinnableSlice _buffer;

    FC_ASSERT( column_types.size() && "read account from rocksdb storage" );
    if( !rocksdb_reader_helper::read<account_name_slice_t, account_name_type::Storage>( provider, key.data, column_types[0], _buffer ) )
      return nullptr;

    rocksdb_account_object _obj;

    load( _obj, _buffer.data(), _buffer.size() );

    return _obj.build( db );
  }
};

template<>
struct rocksdb_reader<account_object, account_id_type>
{
  static const account_object* read( chainbase::database& db, const external_storage_reader_writer::ptr& provider, const account_id_type& key, const std::vector<ColumnTypes>& column_types )
  {
    account_name_type _name;

    FC_ASSERT( column_types.size() == 2 && "read an account from rocksdb storage" );

    {
      PinnableSlice _buffer;

      if( !rocksdb_reader_helper::read<uint32_slice_t, uint32_t>( provider, key, column_types[0], _buffer ) )
        return nullptr;

      rocksdb_account_object_by_id _obj;
      load( _obj, _buffer.data(), _buffer.size() );
      _name = _obj.name;
    }

    PinnableSlice _buffer;

    if( !rocksdb_reader_helper::read<account_name_slice_t, account_name_type::Storage>( provider, _name.data, column_types[1], _buffer ) )
      return nullptr;

    rocksdb_account_object _obj;
    load( _obj, _buffer.data(), _buffer.size() );

    return _obj.build( db );
  }
};

template<typename SHM_Object_Type>
struct updater
{
  static uint32_t get_block_num( const chainbase::database& db )
  {
    auto _found_dgpo = db.find< dynamic_global_property_object >();
    return _found_dgpo ? _found_dgpo->head_block_number : 0;
  }

  static void modify( chainbase::database& db, const SHM_Object_Type& obj )
  {
    db.modify<SHM_Object_Type>( obj, [&]( SHM_Object_Type& o )
    {
      o.set_block_number( get_block_num( db ) );
    } );
  }

  static void modify( chainbase::database& db, const SHM_Object_Type& obj, std::function<void(SHM_Object_Type&)> modifier )
  {
    db.modify<SHM_Object_Type>( obj, [&]( SHM_Object_Type& o )
    {
      modifier( o );
      o.set_block_number( get_block_num( db ) );
    } );
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
  const bfs::path& storage_path, appbase::application& app )
  : db( db )
{
  provider = std::make_shared<rocksdb_account_storage_provider>( blockchain_storage_path, storage_path, app );
  snapshot = std::make_shared<rocksdb_snapshot>( "Accounts RocksDB", "accounts_rocksdb_data", db, storage_path, provider );
}

rocksdb_account_archive::~rocksdb_account_archive()
{
  close();
}

const chainbase::database& rocksdb_account_archive::get_db() const
{
  return db;
}

external_storage_reader_writer::ptr rocksdb_account_archive::get_external_storage_reader_writer() const
{
  return provider;
}

//std::vector<account_name_type> names;

template<typename SHM_Index_Type, typename SHM_Object_Type,
        typename RocksDB_Object_Type,
        typename RocksDB_Object_Type2 = RocksDB_Object_Type>
bool rocksdb_account_archive::on_irreversible_block_impl( uint32_t block_num, const std::vector<ColumnTypes>& column_types )
{
  const auto& _idx = db.get_index<SHM_Index_Type, by_block>();

  auto time_start = std::chrono::high_resolution_clock::now();

  auto _itr = _idx.begin();

#ifdef DBG_MOVE_INFO
  ilog( "rocksdb_account_archive: volatile index size: ${size} for block: ${block_num}", (block_num)("size", _volatile_idx.size()) );
#endif

  bool _do_flush = false;
  uint32_t _cnt = 0;

  /*
    Regarding: `itr->get_block_number() < block_num`
    Here must be `<` not `<=` because `get_block_number` is a number of last block, but `block_num` is the current block.
  */
  while( _itr != _idx.end() && _itr->get_block_number() < block_num )
  {
    const auto& _current = *_itr;
    ++_itr;

    if( _current.changed() )
    {
      ++_cnt;

      transporter<SHM_Object_Type, RocksDB_Object_Type, RocksDB_Object_Type2>::move_to_external_storage( provider, _current, column_types );

      if( !_do_flush )
        _do_flush = true;
    }

    db.remove( _current );
  }

  accounts_stats::stats.account_moved_to_storage.time_ns += std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
  accounts_stats::stats.account_moved_to_storage.count += _cnt;

  return _do_flush;
}

void rocksdb_account_archive::on_irreversible_block( uint32_t block_num )
{
  provider->update_lib( block_num );

  if( objects_limit )
  {
    size_t _nr_elements = 0;
    _nr_elements += db.get_index<account_metadata_index, by_block>().size();
    _nr_elements += db.get_index<account_authority_index, by_block>().size();
    _nr_elements += db.get_index<account_index, by_block>().size();

    if( _nr_elements < objects_limit )
      return;
  }

  bool _do_flush_meta = on_irreversible_block_impl<
                          account_metadata_index, account_metadata_object, rocksdb_account_metadata_object>
                          ( block_num, { ColumnTypes::ACCOUNT_METADATA } );

  bool _do_flush_authority = on_irreversible_block_impl<
                          account_authority_index, account_authority_object, rocksdb_account_authority_object>
                          ( block_num, { ColumnTypes::ACCOUNT_AUTHORITY } );

  bool _do_flush_account = on_irreversible_block_impl
                          <account_index, account_object, rocksdb_account_object, rocksdb_account_object_by_id>
                          ( block_num, { ColumnTypes::ACCOUNT, ColumnTypes::ACCOUNT_BY_ID } );

  if( _do_flush_meta || _do_flush_authority || _do_flush_account )
  {
    auto time_start = std::chrono::high_resolution_clock::now();

    provider->flush();

    accounts_stats::stats.account_flush_to_storage.time_ns += std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
    ++accounts_stats::stats.account_flush_to_storage.count;
  }
}

template<typename Key_Type, typename SHM_Object_Type, typename SHM_Object_Sub_Index>
const SHM_Object_Type* rocksdb_account_archive::get_object( const Key_Type& key, const std::vector<ColumnTypes>& column_types, bool is_required ) const
{
  const auto* _found = db.find<SHM_Object_Type, SHM_Object_Sub_Index>( key );
  if( _found )
  {
    return _found;
  }
  else
  {
    const auto* _external_found = rocksdb_reader<SHM_Object_Type, Key_Type>::read( db, provider, key, column_types );
    if( !_external_found )
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

  updater<account_metadata_object>::modify( db, obj );

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

  return get_object<account_name_type, account_metadata_object, by_name>( account_name, { ColumnTypes::ACCOUNT_METADATA }, account_metadata_is_required );
}

void rocksdb_account_archive::modify_object( const account_metadata_object& obj, std::function<void(account_metadata_object&)>&& modifier )
{
  auto time_start = std::chrono::high_resolution_clock::now();

  updater<account_metadata_object>::modify( db, obj, modifier );

  accounts_stats::stats.account_metadata_modified.time_ns += std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
  ++accounts_stats::stats.account_metadata_modified.count;
}
//==========================================account_metadata_object==========================================

//==========================================account_authority_object==========================================
void rocksdb_account_archive::create_object( const account_authority_object& obj )
{
  auto time_start = std::chrono::high_resolution_clock::now();

  updater<account_authority_object>::modify( db, obj );

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

  return get_object<account_name_type, account_authority_object, by_name>( account_name, { ColumnTypes::ACCOUNT_AUTHORITY }, account_authority_is_required );
}

void rocksdb_account_archive::modify_object( const account_authority_object& obj, std::function<void(account_authority_object&)>&& modifier )
{
  auto time_start = std::chrono::high_resolution_clock::now();

  updater<account_authority_object>::modify( db, obj, modifier );

  accounts_stats::stats.account_authority_modified.time_ns += std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
  ++accounts_stats::stats.account_authority_modified.count;
}
//==========================================account_authority_object==========================================

//==========================================account_object==========================================
void rocksdb_account_archive::create_object( const account_object& obj )
{
  auto time_start = std::chrono::high_resolution_clock::now();

  updater<account_object>::modify( db, obj );
  db.create< tiny_account_object >( obj );

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

  return get_object<account_name_type, account_object, by_name>( account_name, { ColumnTypes::ACCOUNT }, account_is_required );
}

const account_object* rocksdb_account_archive::get_account( const account_id_type& account_id, bool account_is_required ) const
{
  auto time_start = std::chrono::high_resolution_clock::now();

  BOOST_SCOPE_EXIT_ALL(&)
  {
    accounts_stats::stats.account_accessed_by_id.time_ns += std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
    ++accounts_stats::stats.account_accessed_by_id.count;
  };

  return get_object<account_id_type, account_object, by_account_id>( account_id, { ColumnTypes::ACCOUNT_BY_ID, ColumnTypes::ACCOUNT }, account_is_required );
}

void rocksdb_account_archive::modify_object( const account_object& obj, std::function<void(account_object&)>&& modifier )
{
  auto time_start = std::chrono::high_resolution_clock::now();

  auto _previous_proxy  = obj.get_proxy();
  auto _previous_next_vesting_withdrawal  = obj.get_next_vesting_withdrawal();
  auto _previous_governance_vote_expiration_ts  = obj.get_governance_vote_expiration_ts();
  auto _previous_delayed_votes = obj.get_delayed_votes();

  updater<account_object>::modify( db, obj, modifier );

  auto _new_proxy = obj.get_proxy();
  auto _new_next_vesting_withdrawal = obj.get_next_vesting_withdrawal();
  auto _new_governance_vote_expiration_ts = obj.get_governance_vote_expiration_ts();
  auto _new_delayed_votes = obj.get_delayed_votes();

  if(
      _previous_proxy != _new_proxy ||
      _previous_next_vesting_withdrawal != _new_next_vesting_withdrawal ||
      _previous_governance_vote_expiration_ts != _new_governance_vote_expiration_ts ||
      _previous_delayed_votes != _new_delayed_votes
    )
  {
    const auto& _idx = db.get_index<tiny_account_index, by_name>();
    auto _found = _idx.find( obj.get_name() );
    FC_ASSERT( _found != _idx.end() );

    db.modify( *_found, [&]( tiny_account_object& o )
    {
      o.modify( obj );
    } );
  }

  accounts_stats::stats.account_modified.time_ns += std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
  ++accounts_stats::stats.account_modified.count;
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
  // volatile_comment_index is registered in database, so it is handled automatically
  provider->openDb( db.get_last_irreversible_block_num() );
}

void rocksdb_account_archive::close()
{
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
