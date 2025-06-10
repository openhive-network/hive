
#include <hive/chain/hive_object_types.hpp>
#include <hive/chain/index.hpp>

#include <hive/chain/util/type_registrar_definition.hpp>

#include <hive/chain/external_storage/comment_rocksdb_objects.hpp>
#include <hive/chain/external_storage/rocksdb_comment_archive.hpp>
#include <hive/chain/external_storage/utilities.hpp>
#include <hive/chain/external_storage/rocksdb_comment_storage_provider.hpp>
#include <hive/chain/external_storage/rocksdb_snapshot.hpp>
#include <hive/chain/external_storage/state_snapshot_provider.hpp>

namespace hive { namespace chain {

//#define DBG_INFO
//#define DBG_MOVE_INFO
//#define DBG_MOVE_DETAILS_INFO

rocksdb_comment_archive::rocksdb_comment_archive( database& db, const bfs::path& blockchain_storage_path,
  const bfs::path& storage_path, appbase::application& app, bool destroy_on_startup, bool destroy_on_shutdown )
  : db( db ), destroy_database_on_startup( destroy_on_startup ), destroy_database_on_shutdown( destroy_on_shutdown )
{
  HIVE_ADD_PLUGIN_INDEX( db, volatile_comment_index );
  provider = std::make_shared<rocksdb_comment_storage_provider>( blockchain_storage_path, storage_path, app );
  snapshot = std::make_shared<rocksdb_snapshot>( "Comments RocksDB", "comments_rocksdb_data", db, storage_path, provider );
}

rocksdb_comment_archive::~rocksdb_comment_archive()
{
  close();
}

void rocksdb_comment_archive::on_cashout( const comment_object& _comment, const comment_cashout_object& _comment_cashout )
{
  auto time_start = std::chrono::high_resolution_clock::now();

  const auto& _volatile_idx = db.get_index< volatile_comment_index, by_permlink >();
  FC_ASSERT( _volatile_idx.find( _comment.get_author_and_permlink_hash() ) == _volatile_idx.end(),
    "Incorrect duplicate archiving of the same comment" ); //ABW: because volatile_comment_index is under undo, this should never happen

#ifdef DBG_INFO
  auto& _account = db.get_account( _comment_cashout.get_author_id() );
  ilog( "head: ${head} lib: ${lib} Store a comment with hash: ${hash}, with author/permlink: ${author}/${permlink}",
    ( "head", db.head_block_num() )( "lib", db.get_last_irreversible_block_num() )
    ( "hash", _comment.get_author_and_permlink_hash() )
    ( "author", _account.get_name() )( "permlink", _comment_cashout.get_permlink() ) );
#endif

  db.create< volatile_comment_object >( [&]( volatile_comment_object& o )
  {
    o.comment_id      = _comment.get_id();
    o.parent_comment  = _comment.get_parent_id();
    o.depth           = _comment.get_depth();
    o.set_author_and_permlink_hash( _comment.get_author_and_permlink_hash() );

    o.block_number    = db.head_block_num();
  });

  stats.comment_cashout_processing.time += std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
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

  return std::shared_ptr<comment_object>( new comment_object ( _obj.comment_id, _obj.parent_comment, hash, _obj.depth ) );
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

  bool _do_flush = false;

  uint64_t count = 0;
  while( _itr != _volatile_idx.end() && _itr->block_number <= block_num )
  {
    const auto& _current = *_itr;
    ++_itr;

    move_to_external_storage_impl( block_num, _current );

    if( !_do_flush )
      _do_flush = true;

    const auto* _comment = db.find_comment( _current.comment_id );
    FC_ASSERT( _comment );

    db.remove( *_comment );
    db.remove( _current );

    ++count;
  }

  if( _do_flush )
    provider->flush();

  stats.comment_lib_processing.time += std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
  stats.comment_lib_processing.count += count;
}

comment rocksdb_comment_archive::get_comment( const account_id_type& author, const std::string& permlink, bool comment_is_required ) const
{
  auto time_start = std::chrono::high_resolution_clock::now();

  auto _hash = comment_object::compute_author_and_permlink_hash( author, permlink );
  const auto* _comment = db.find< comment_object, by_permlink >( _hash );
  if( _comment )
  {
    stats.comment_accessed_from_index.time += std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
    ++stats.comment_accessed_from_index.count;
    return comment( _comment );
  }
  else
  {
    const auto _external_comment = get_comment_impl( _hash );
    uint64_t time = std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
    if( _external_comment )
    {
      stats.comment_accessed_from_archive.time += time;
      ++stats.comment_accessed_from_archive.count;
    }
    else
    {
      stats.comment_not_found.time += time;
      ++stats.comment_not_found.count;
      FC_ASSERT( !comment_is_required, "Comment with `id`/`permlink` ${author}/${permlink} not found", ( author ) ( permlink ) );
    }
    return comment( _external_comment );
  }
}

void rocksdb_comment_archive::save_snaphot( const hive::chain::prepare_snapshot_supplement_notification& note )
{
  snapshot->save_snaphot( note );
}

void rocksdb_comment_archive::load_snapshot( const hive::chain::load_snapshot_supplement_notification& note )
{
  snapshot->load_snapshot( note );
}

void rocksdb_comment_archive::open()
{
  provider->init( destroy_database_on_startup );
}

void rocksdb_comment_archive::close()
{
  provider->shutdownDb( destroy_database_on_shutdown );
}

void rocksdb_comment_archive::wipe()
{
  provider->wipeDb();
}

void rocksdb_comment_archive::update_lib( uint32_t lib )
{
  provider->update_lib( lib );
}

void rocksdb_comment_archive::update_reindex_point( uint32_t rp )
{
  provider->update_reindex_point( rp );
}

}}

HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE( hive::chain::volatile_comment_index )
