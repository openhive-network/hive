
#include <hive/chain/hive_object_types.hpp>
#include <hive/chain/external_storage/rocksdb_storage_processor.hpp>
#include <hive/chain/external_storage/utilities.hpp>

namespace hive { namespace chain {

bool dbg_info = false;
bool dbg_move_info = true;

rocksdb_storage_processor::rocksdb_storage_processor( database& db, const external_storage_provider::ptr& provider ): db( db ), provider( provider )
{
}

void rocksdb_storage_processor::store_comment( const account_name_type& author, const std::string& permlink )
{
  auto& _account = db.get_account( author );
  auto _found = db.get_comment( author, permlink );

  FC_ASSERT( _found, "Comment ${permlink}/${author} has to exist", ("permlink", permlink)("author", author) );

  const auto& _volatile_idx = db.get_index< volatile_comment_index, by_permlink >();
  auto _vfound = _volatile_idx.find( _found.get_author_and_permlink_hash() );

  if( _vfound != _volatile_idx.end() )
    return;

  if( dbg_info )
  {
    ilog( "rocksdb_storage_processor: head: ${head} lib: ${lib} Store a comment with hash: ${hash}, with permlink/author: ${permlink}/${author}",
    ("hash", comment_object::compute_author_and_permlink_hash( _account.get_id(), permlink ))
    ("permlink", permlink)("author", author)("head", db.head_block_num())("lib", db.get_last_irreversible_block_num()) );
  }

  db.create< volatile_comment_object >( [&]( volatile_comment_object& o )
  {
    o.comment_id      = _found.get_id();
    o.parent_comment  = _found.get_parent_id();
    o.depth           = _found.get_depth(); 
    o.set_author_and_permlink_hash( _found.get_author_and_permlink_hash() );

    o.block_number    = db.head_block_num();
  });
}

void rocksdb_storage_processor::delete_comment( const account_name_type& author, const std::string& permlink )
{
  auto& _account = db.get_account( author );
  auto _found = db.get_comment( author, permlink );

  if( _found )
    return;

  const auto& _volatile_idx = db.get_index< volatile_comment_index, by_permlink >();

  auto _hash = comment_object::compute_author_and_permlink_hash( _account.get_id(), permlink );

  auto _vfound = _volatile_idx.find( _hash );

  if( _vfound == _volatile_idx.end() )
    return;

  if( dbg_info )
  {
    ilog( "rocksdb_storage_processor: head: ${head} lib: ${lib} Delete a comment with hash: ${hash}, with permlink/author: ${permlink}/${author}",
    ("hash", _hash)
    ("permlink", permlink)("author", author)("head", db.head_block_num())("lib", db.get_last_irreversible_block_num()) );
  }

  db.remove( *_vfound );

  //Slice _key( _hash.data(), _hash.data_size() );
  //provider->remove( _key, 0/*column_number*/ );
}

void rocksdb_storage_processor::allow_move_to_external_storage( const account_id_type& account_id, const shared_string& permlink )
{
  auto& _account = db.get_account( account_id );

  if( dbg_info )
  {
    ilog("rocksdb_storage_processor: A comment with hash: ${hash} permlink/author: ${permlink}/${author} was paid.",
          ("hash", comment_object::compute_author_and_permlink_hash( account_id, permlink.c_str()))
          ("permlink", permlink)("author", _account.get_name()) );
  }

  const auto& _volatile_idx = db.get_index< volatile_comment_index, by_permlink >();
  auto _found = _volatile_idx.find( comment_object::compute_author_and_permlink_hash( account_id, permlink.c_str()) );
  FC_ASSERT( _found != _volatile_idx.end() );

  db.modify( *_found, []( volatile_comment_object& vc )
  {
    vc.was_paid = true;
  });
}

void rocksdb_storage_processor::move_to_external_storage_impl( uint32_t block_num, const volatile_comment_object& volatile_object )
{
  if( dbg_info )
  {
    ilog( "rocksdb_storage_processor: lib ${lib} Move to external storage a comment with id: ${comment_id}, hash: ${hash}",
          ("comment_id", volatile_object.comment_id)("hash", volatile_object.get_author_and_permlink_hash())("lib", block_num) );
  }

  Slice _key( volatile_object.get_author_and_permlink_hash().data(), volatile_object.get_author_and_permlink_hash().data_size() );

  rocksdb_comment_object _obj( volatile_object );

  auto _serialize_buffer = dump( _obj );
  Slice _value( _serialize_buffer.data(), _serialize_buffer.size() );

  provider->save( _key, _value, 0/*column_number*/ );
}

std::shared_ptr<comment_object> rocksdb_storage_processor::get_comment_impl( const account_id_type& author, const std::string& permlink ) const
{
  auto _hash = comment_object::compute_author_and_permlink_hash( author, permlink );

  Slice _key( _hash.data(), _hash.data_size() );

  PinnableSlice _buffer;

  bool _status = provider->read( _key, _buffer, 0/*column_number*/ );

  if( !_status )
    return std::shared_ptr<comment_object>();

  rocksdb_comment_object _obj;

  load( _obj, _buffer.data(), _buffer.size() );

  return std::shared_ptr<comment_object>( new comment_object ( _obj.id,
                                                  oid_ref< comment_object>( oid<comment_object>( _obj.parent_comment ) ),
                                                   _hash, _obj.depth ) );
}

void rocksdb_storage_processor::move_to_external_storage( uint32_t block_num )
{
  if( !db.has_hardfork( HIVE_HARDFORK_0_19 ) )
    return;

  const auto& _volatile_idx = db.get_index< volatile_comment_index, by_block >();

  auto _itr = _volatile_idx.begin();
  auto _itr_end = _volatile_idx.upper_bound( block_num );

  if( _volatile_idx.size() < volatile_objects_limit )
    return;
 
  if( dbg_move_info )
  {
    ilog( "rocksdb_storage_processor: volatile index size: ${size} for block: ${block_num}", (block_num)("size", _volatile_idx.size()) );
  }

  bool _do_flush = false;

  while( _itr != _itr_end )
  {
    const auto& _current = *_itr;
    ++_itr;

    if( !_current.was_paid )
      continue;

    move_to_external_storage_impl( block_num, _current );

    if( !_do_flush )
      _do_flush = true;

    const auto& _comment = db.get_comment( _current.comment_id );
    db.remove( _comment );
    db.remove( _current );
  }

  if( _do_flush )
    provider->flush();
}

comment rocksdb_storage_processor::get_comment( const account_id_type& author, const std::string& permlink ) const
{
  const auto* _comment = db.find< comment_object, by_permlink >( comment_object::compute_author_and_permlink_hash( author, permlink ) );
  if( _comment )
    return comment( _comment );
  else
    return comment( get_comment_impl( author, permlink ) );
}

comment rocksdb_storage_processor::get_comment( const account_name_type& author, const std::string& permlink ) const
{
  const account_object* _account = db.find_account( author );
  if( !_account )
    return nullptr;

  return get_comment( _account->get_id(), permlink );
}

}}
