
#include <hive/chain/hive_object_types.hpp>
#include <hive/chain/external_storage/rocksdb_storage_processor.hpp>
#include <hive/chain/external_storage/utilities.hpp>

namespace hive { namespace chain {

bool dbg_info = true;

rocksdb_storage_processor::rocksdb_storage_processor( database& db, const external_storage_provider::ptr& provider ): db( db ), provider( provider )
{
}

void rocksdb_storage_processor::store_comment( const account_name_type& author, const std::string& permlink )
{
  auto& _account = db.get_account( author );
  auto _found = db.find_comment( author, permlink );

  FC_ASSERT( _found, "Comment ${permlink}/${author} has to exist", ("permlink", permlink)("author", author) );

  const auto& _volatile_idx = db.get_index< volatile_comment_index, by_permlink >();
  auto _vfound = _volatile_idx.find( _found->get_author_and_permlink_hash() );

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
    o.comment_id = _found->get_id();
    o.block_number = db.head_block_num();
    o.set_author_and_permlink_hash( _found->get_author_and_permlink_hash() );
  });
}

void rocksdb_storage_processor::delete_comment( const account_name_type& author, const std::string& permlink )
{
  auto& _account = db.get_account( author );
  auto _found = db.find_comment( author, permlink );

  FC_ASSERT( _found == nullptr, "Comment ${permlink}/${author} has to be deleted", ("permlink", permlink)("author", author) );

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

  Slice _key( _hash.data(), _hash.data_size() );
  provider->remove( _key, 0/*column_number*/ );
}

void rocksdb_storage_processor::comment_was_paid( const account_id_type& account_id, const shared_string& permlink )
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

  const auto& _comment = db.get_comment( _found->comment_id );

  db.modify( *_found, [&_comment]( volatile_comment_object& vc )
  {
    vc.parent_comment           = _comment.get_parent_id();
    vc.depth                    = _comment.get_depth(); 

    vc.was_paid                 = true;
  });
}

void rocksdb_storage_processor::move_to_external_storage_impl( uint32_t block_num, const volatile_comment_object& volatile_object )
{
  //temporary disabled!!!!
  // if( !volatile_object.was_paid )
  //   return;

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

void rocksdb_storage_processor::move_to_external_storage( uint32_t block_num )
{
  const auto& _volatile_idx = db.get_index< volatile_comment_index, by_block >();

  auto _it = _volatile_idx.find( block_num );

  while( _it != _volatile_idx.end() && _it->block_number <= block_num )
  {
    move_to_external_storage_impl( block_num, *_it );

    //temporary disabled!!!!
    //const auto& _comment = db.get_comment( _it->comment_id );
    //db.remove( _comment );

    ++_it;
  }
}

std::shared_ptr<comment_object> rocksdb_storage_processor::find_comment( const account_id_type& author, const std::string& permlink )
{
  auto _hash = comment_object::compute_author_and_permlink_hash( author, permlink );

  Slice _key( _hash.data(), _hash.data_size() );

  std::string _buffer;

  provider->read( _key, _buffer, 0/*column_number*/ );

  rocksdb_comment_object _obj;

  load( _obj, _buffer.data(), _buffer.size() );

  return std::shared_ptr<comment_object>( new comment_object ( _obj.id,
                                                  oid_ref< comment_object>( oid<comment_object>( _obj.parent_comment ) ),
                                                   _hash, _obj.depth ) );
}

}}
