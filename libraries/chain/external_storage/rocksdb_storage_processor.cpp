
#include <hive/chain/hive_object_types.hpp>

#include <hive/chain/external_storage/rocksdb_storage_processor.hpp>
#include <hive/chain/external_storage/utilities.hpp>
#include <hive/chain/external_storage/rocksdb_storage_provider.hpp>
#include <hive/chain/external_storage/rocksdb_snapshot.hpp>
#include <hive/chain/external_storage/state_snapshot_provider.hpp>

namespace hive { namespace chain {

//#define DBG_INFO
//#define DBG_MOVE_INFO
//#define DBG_MOVE_DETAILS_INFO

rocksdb_storage_processor::rocksdb_storage_processor( const abstract_plugin& plugin, database& db, const bfs::path& blockchain_storage_path, const bfs::path& storage_path, appbase::application& app, bool destroy_on_startup )
                          : db( db )
{
  auto _rcs_provider = std::make_shared<rocksdb_comment_storage_provider>( blockchain_storage_path, storage_path, app );
  _rcs_provider->init( destroy_on_startup );

  provider = _rcs_provider;
  snapshot = std::shared_ptr<rocksdb_snapshot>( new rocksdb_snapshot( "Comments RocksDB", "comments_rocksdb_data", plugin, db, storage_path, provider ) );
}

rocksdb_storage_processor::~rocksdb_storage_processor()
{
}

void rocksdb_storage_processor::allow_move_to_external_storage( const comment_id_type& comment_id, const account_id_type& account_id, const std::string& permlink )
{
  auto& _account = db.get_account( account_id );
  auto _found = db.find_comment( comment_id );

  FC_ASSERT( _found, "Comment ${author}/${permlink} has to exist", ("permlink", permlink)("author", _account.get_name()) );

  const auto& _volatile_idx = db.get_index< volatile_comment_index, by_permlink >();
  auto _vfound = _volatile_idx.find( _found->get_author_and_permlink_hash() );

  if( _vfound != _volatile_idx.end() )
    return;

#ifdef DBG_INFO
  ilog( "head: ${head} lib: ${lib} Store a comment with hash: ${hash}, with author/permlink: ${author}/${permlink}",
  ("hash", comment_object::compute_author_and_permlink_hash( _account.get_id(), permlink ))
  ("permlink", permlink)("author", _account.get_name())("head", db.head_block_num())("lib", db.get_last_irreversible_block_num()) );
#endif

  db.create< volatile_comment_object >( [&]( volatile_comment_object& o )
  {
    o.comment_id      = _found->get_id();
    o.parent_comment  = _found->get_parent_id();
    o.depth           = _found->get_depth();
    o.set_author_and_permlink_hash( _found->get_author_and_permlink_hash() );

    o.block_number    = db.head_block_num();
  });
}

void rocksdb_storage_processor::move_to_external_storage_impl( uint32_t block_num, const volatile_comment_object& volatile_object )
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

std::shared_ptr<comment_object> rocksdb_storage_processor::get_comment_impl( const comment_object::author_and_permlink_hash_type& hash ) const
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

void rocksdb_storage_processor::move_to_external_storage( uint32_t block_num )
{
  if( !db.has_hardfork( HIVE_HARDFORK_0_19 ) )
    return;

  const auto& _volatile_idx = db.get_index< volatile_comment_index, by_block >();

  if( _volatile_idx.size() < volatile_objects_limit )
    return;

  auto _itr = _volatile_idx.begin();

#ifdef DBG_MOVE_INFO
  ilog( "rocksdb_storage_processor: volatile index size: ${size} for block: ${block_num}", (block_num)("size", _volatile_idx.size()) );
#endif

  bool _do_flush = false;

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
  }

  if( _do_flush )
    provider->flush();
}

comment rocksdb_storage_processor::get_comment( const account_id_type& author, const std::string& permlink, bool comment_is_required ) const
{
  auto _hash = comment_object::compute_author_and_permlink_hash( author, permlink );
  const auto* _comment = db.find< comment_object, by_permlink >( _hash );
  if( _comment )
  {
    return comment( _comment );
  }
  else
  {
    const auto _external_comment = get_comment_impl( _hash );
    FC_ASSERT( !comment_is_required || _external_comment, "Comment with `id`/`permlink` ${author}/${permlink} not found", (author)(permlink) );
    return comment( _external_comment );
  }
}

comment rocksdb_storage_processor::get_comment( const account_name_type& author, const std::string& permlink, bool comment_is_required ) const
{
  const account_object* _account = db.find_account( author );
  if( !_account )
    return nullptr;

  return get_comment( _account->get_id(), permlink, comment_is_required );
}

void rocksdb_storage_processor::supplement_snapshot( const hive::chain::prepare_snapshot_supplement_notification& note )
{
  snapshot->supplement_snapshot( note );
}

void rocksdb_storage_processor::load_additional_data_from_snapshot( const hive::chain::load_snapshot_supplement_notification& note )
{
  snapshot->load_additional_data_from_snapshot( note );
}

void rocksdb_storage_processor::shutdown( bool remove_db )
{
  provider->shutdownDb( remove_db );
}

}}
