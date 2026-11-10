
#include <hive/chain/hive_object_types.hpp>
#include <hive/chain/index.hpp>
#include <chainbase/chainbase.inl>
#include <hive/chain/detail/state/comment_object_multiindex.hpp>
#include <hive/chain/detail/state/account_object.hpp>

#include <hive/chain/util/type_registrar_definition.hpp>

#include <hive/chain/external_storage/comment_rocksdb_objects_multiindex.hpp>
#include <hive/chain/external_storage/rocksdb_comment_archive.hpp>
#include <hive/chain/external_storage/utilities.hpp>
#include <hive/chain/external_storage/rocksdb_comment_storage_provider.hpp>
#include <hive/chain/external_storage/rocksdb_snapshot.hpp>
#include <hive/chain/external_storage/state_snapshot_provider.hpp>

namespace hive { namespace chain {

//#define DBG_INFO
//#define DBG_MOVE_INFO
//#define DBG_MOVE_DETAILS_INFO

namespace {

/// Replay reads up to 1000 blocks ahead of the one being applied (block_log::for_each_block), and
/// entries must last until their block is applied; a block rarely needs more than a few hundred.
constexpr size_t PREFETCHED_COMMENTS_CAPACITY = 256 * 1024;
constexpr size_t PREFETCH_AUTHORS_CAPACITY = 1024 * 1024;

template< typename... Counters >
void count_lookup( uint64_t time_ns, Counters&... counters )
{
  ( ( counters.time_ns += time_ns, ++counters.count ), ... );
}

std::shared_ptr<comment_object> make_archived_comment( const comment_object::author_and_permlink_hash_type& hash, const rocksdb_comment_object& obj )
{
  return std::shared_ptr<comment_object>( new comment_object ( obj.comment_id, obj.parent_comment, hash, obj.depth ) );
}

}

rocksdb_comment_archive::rocksdb_comment_archive( database& db, const bfs::path& blockchain_storage_path, const bfs::path& storage_path, appbase::application& app,
  size_t block_cache_size, bool enable_prefetch )
  : db( db ), prefetch_enabled( enable_prefetch ),
    prefetched_comments( PREFETCHED_COMMENTS_CAPACITY ), author_ids( PREFETCH_AUTHORS_CAPACITY )
{
  HIVE_ADD_PLUGIN_INDEX( db, volatile_comment_index );
  provider = std::make_shared<rocksdb_comment_storage_provider>( blockchain_storage_path, storage_path, app, block_cache_size );
  snapshot = std::make_shared<rocksdb_snapshot>( "Comments RocksDB", "comments_rocksdb_data", db, storage_path, provider );
}

rocksdb_comment_archive::~rocksdb_comment_archive()
{
  close();
}

void rocksdb_comment_archive::on_cashout( uint32_t _block_num, const comment_object& _comment, const comment_cashout_object& _comment_cashout )
{
  auto time_start = std::chrono::high_resolution_clock::now();

  const auto& _volatile_idx = db.get_index< volatile_comment_index, by_permlink >();
  FC_ASSERT( _volatile_idx.find( _comment.get_author_and_permlink_hash() ) == _volatile_idx.end(),
    "Incorrect duplicate archiving of the same comment" ); //ABW: because volatile_comment_index is under undo, this should never happen

#ifdef DBG_INFO
  auto& _account = db.get_account( _comment_cashout.get_author_id() );
  ilog( "head: ${_block_num} lib: ${lib} Store a comment with hash: ${hash}, with author/permlink: ${author}/${permlink}",
    ( _block_num )( "lib", db.get_last_irreversible_block_num() )
    ( "hash", _comment.get_author_and_permlink_hash() )
    ( "author", _account.get_name() )( "permlink", _comment_cashout.get_permlink() ) );
#endif

  db.create< volatile_comment_object >( [&]( volatile_comment_object& o )
  {
    o.comment_id      = _comment.get_id();
    o.parent_comment  = _comment.get_parent_id();
    o.depth           = _comment.get_depth();
    o.set_author_and_permlink_hash( _comment.get_author_and_permlink_hash() );

    o.block_number    = _block_num;
  });

  if( prefetch_enabled )
    remember_author( _comment_cashout.get_author_id() );

  stats.comment_cashout_processing.time_ns += std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
  ++stats.comment_cashout_processing.count;
}

void rocksdb_comment_archive::move_to_external_storage_impl( uint32_t block_num, const volatile_comment_object& volatile_object )
{
#ifdef DBG_MOVE_DETAILS_INFO
  ilog( "lib ${lib} Move to external storage a comment with id: ${comment_id}, hash: ${hash}",
        ("comment_id", volatile_object.comment_id)("hash", volatile_object.get_author_and_permlink_hash())("lib", block_num) );
#endif

  Slice _key( volatile_object.get_author_and_permlink_hash().data(), volatile_object.get_author_and_permlink_hash().data_size() );

  rocksdb_comment_object _obj( volatile_object );

  auto _serialize_buffer = dump( _obj );
  Slice _value( _serialize_buffer.data(), _serialize_buffer.size() );

  provider->save( _key, _value );
}

std::shared_ptr<comment_object> rocksdb_comment_archive::get_comment_impl( const comment_object::author_and_permlink_hash_type& hash ) const
{
  Slice _key( hash.data(), hash.data_size() );

  PinnableSlice _buffer;

  bool _status = provider->read( _key, _buffer );

  if( !_status )
    return std::shared_ptr<comment_object>();

  rocksdb_comment_object _obj;

  load( _obj, _buffer.data(), _buffer.size() );

  return make_archived_comment( hash, _obj );
}

void rocksdb_comment_archive::remember_author( const account_id_type& author ) const
{
  author_ids.remember( author, [&]() { return db.get_account( author ).get_name(); } );
}

std::shared_ptr<comment_object> rocksdb_comment_archive::get_prefetched_comment( const comment_object::author_and_permlink_hash_type& hash ) const
{
  // Only entries found in the archive are prefetched, never their absence: a comment can move
  // into the archive (on_irreversible_block) after the prefetch looked for it.
  const auto _entry = prefetched_comments.find( hash );
  if( !_entry )
    return std::shared_ptr<comment_object>();
  return make_archived_comment( hash, *_entry );
}

void rocksdb_comment_archive::prefetch_comments( const std::vector<full_transaction_ptr>& transactions )
{
  if( !prefetch_enabled )
    return;

  std::vector<comment_lookup_key> _keys;
  for( const auto& _transaction : transactions )
    collect_comment_lookup_keys( _transaction->get_transaction(), _keys );

  std::vector<comment_object::author_and_permlink_hash_type> _hashes;
  _hashes.reserve( _keys.size() );
  for( const auto& _key : _keys )
  {
    // an author not known yet (also one created in the same block) is left to the normal lookup
    const auto _author = author_ids.find( _key.author );
    if( _author )
      _hashes.push_back( comment_object::compute_author_and_permlink_hash( *_author, _key.permlink ) );
  }
  std::sort( _hashes.begin(), _hashes.end() );
  _hashes.erase( std::unique( _hashes.begin(), _hashes.end() ), _hashes.end() );

  prefetched_comments.keep_missing( _hashes );
  if( _hashes.empty() )
    return;

  std::shared_lock<std::shared_mutex> _lock( storage_mutex );
  if( !provider->getStorage() )
    return;

  std::vector<Slice> _slices;
  _slices.reserve( _hashes.size() );
  for( const auto& _hash : _hashes )
    _slices.emplace_back( _hash.data(), _hash.data_size() );

  std::vector<PinnableSlice> _values;
  std::vector<::rocksdb::Status> _statuses;
  provider->multi_read( _slices, _values, _statuses );

  std::vector<std::pair<comment_object::author_and_permlink_hash_type, rocksdb_comment_object>> _found;
  for( size_t i = 0; i < _hashes.size(); ++i )
  {
    if( !_statuses[i].ok() )
      continue;
    rocksdb_comment_object _obj;
    load( _obj, _values[i].data(), _values[i].size() );
    _found.emplace_back( _hashes[i], _obj );
  }
  // still under the lock, so entries read from a storage that is being replaced cannot outlive clear_prefetched
  prefetched_comments.store( _found );
}

void rocksdb_comment_archive::clear_prefetched()
{
  prefetched_comments.clear();
  author_ids.clear();
}

void rocksdb_comment_archive::on_irreversible_block( uint32_t block_num )
{
  provider->update_lib( block_num );

  if( !db.has_hardfork( HIVE_HARDFORK_0_19 ) )
    return;

  const auto& _volatile_idx = db.get_index< volatile_comment_index, by_block >();

  if( _volatile_idx.size() < volatile_objects_limit )
    return;

  auto time_start = std::chrono::high_resolution_clock::now();

  auto _itr = _volatile_idx.begin();

#ifdef DBG_MOVE_INFO
  ilog( "rocksdb_comment_archive: volatile index size: ${size} for block: ${block_num}", (block_num)("size", _volatile_idx.size()) );
#endif

  uint64_t count = 0;
  while( _itr != _volatile_idx.end() && _itr->block_number <= block_num )
  {
    const auto& _current = *_itr;
    ++_itr;

    move_to_external_storage_impl( block_num, _current );

    const auto* _comment = db.find_comment( _current.comment_id );
    FC_ASSERT( _comment );

    db.remove( *_comment );
    db.remove( _current );

    ++count;
  }

  provider->flushWriteBuffer();

  stats.comment_lib_processing.time_ns += std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
  stats.comment_lib_processing.count += count;
}

comment rocksdb_comment_archive::get_comment( const account_id_type& author, const std::string& permlink, bool comment_is_required ) const
{
  auto time_start = std::chrono::high_resolution_clock::now();
  auto elapsed = [&]() -> uint64_t
  {
    return std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
  };
  auto& _path_stats = current_path_stats();

  auto _hash = comment_object::compute_author_and_permlink_hash( author, permlink );
  const auto* _comment = db.find< comment_object, by_permlink >( _hash );
  if( _comment )
  {
    count_lookup( elapsed(), stats.comment_accessed_from_index, _path_stats.from_index );
    return comment( _comment );
  }
  else
  {
    auto _external_comment = prefetch_enabled ? get_prefetched_comment( _hash ) : std::shared_ptr<comment_object>();
    const bool _prefetched = static_cast<bool>( _external_comment );
    if( !_prefetched )
    {
      _external_comment = get_comment_impl( _hash );
      // authors are learned here and on cashout rather than on every lookup, which keeps the far more
      // frequent lookups of comments that do not exist anywhere free of the prefetch bookkeeping
      if( _external_comment && prefetch_enabled )
        remember_author( author );
    }
    const uint64_t time = elapsed();
    if( _external_comment )
    {
      count_lookup( time, stats.comment_accessed_from_archive );
      if( _prefetched )
        count_lookup( time, stats.comment_prefetch_hit, _path_stats.prefetch_hit );
      else if( prefetch_enabled )
        count_lookup( time, stats.comment_prefetch_miss, _path_stats.archive_read );
      else
        count_lookup( time, _path_stats.archive_read );
    }
    else
    {
      count_lookup( time, stats.comment_not_found, _path_stats.not_found );
      FC_ASSERT( !comment_is_required, "Comment with `id`/`permlink` ${author}/${permlink} not found", ( author ) ( permlink ) );
    }
    return comment( _external_comment );
  }
}

void rocksdb_comment_archive::save_snapshot( const hive::chain::prepare_snapshot_supplement_notification& note )
{
  snapshot->save_snapshot( note );
}

void rocksdb_comment_archive::load_snapshot( const hive::chain::load_snapshot_supplement_notification& note )
{
  std::unique_lock<std::shared_mutex> _lock( storage_mutex );
  clear_prefetched();
  snapshot->load_snapshot( note );
}

void rocksdb_comment_archive::open()
{
  std::unique_lock<std::shared_mutex> _lock( storage_mutex );
  clear_prefetched();
  // volatile_comment_index is registered in database, so it is handled automatically
  provider->openDb( db.get_last_irreversible_block_num() );
}

void rocksdb_comment_archive::close()
{
  std::unique_lock<std::shared_mutex> _lock( storage_mutex );
  clear_prefetched();
  // volatile_comment_index is registered in database, so it is handled automatically
  provider->shutdownDb();
}

void rocksdb_comment_archive::wipe()
{
  std::unique_lock<std::shared_mutex> _lock( storage_mutex );
  clear_prefetched();
  // volatile_comment_index is registered in database, so it is handled automatically
  provider->wipeDb();
}

void rocksdb_comment_archive::flush( bool force_storage_flush )
{
  // Submitting the pending write batch is what keeps this archive in step with flushed shared
  // memory: on_irreversible_block moves a comment here and then removes it from state, so the
  // batch must reach the WAL before that removal can be persisted. Forcing the memtables out
  // into files is a separate, much more expensive step that only the explicit flush points
  // need (issue #869).
  if( force_storage_flush )
    provider->flushDb();
  else
    provider->flushWriteBuffer();
}

void rocksdb_comment_archive::on_end_of_syncing()
{
  ilog("Remove a limit for comment objects");
  volatile_objects_limit = 0;
}

} }

HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE( hive::chain::volatile_comment_index )

// Explicit template instantiations for chainbase::database methods
template const chainbase::generic_index<hive::chain::volatile_comment_index>& chainbase::database::get_index<hive::chain::volatile_comment_index>() const;
template chainbase::generic_index<hive::chain::volatile_comment_index>& chainbase::database::get_mutable_index<hive::chain::volatile_comment_index>();
