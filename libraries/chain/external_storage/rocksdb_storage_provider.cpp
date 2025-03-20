
#include <hive/chain/hive_object_types.hpp>
#include <hive/chain/external_storage/rocksdb_storage_provider.hpp>
#include <hive/chain/external_storage/utilities.hpp>

namespace hive { namespace chain {

bool dbg_info = true;

rocksdb_storage_provider::rocksdb_storage_provider( const external_storage_mgr::ptr& mgr ): mgr( mgr )
{
}

void rocksdb_storage_provider::store_comment( const hive::protocol::comment_operation& op )
{
  auto& _db = mgr->get_database();

  auto& _account = _db.get_account( op.author );
  auto _found = _db.find_comment( op.author, op.permlink );

  FC_ASSERT( _found, "Comment ${permlink}/${author} has to exist", ("permlink", op.permlink)("author", op.author) );

  const auto& _volatile_idx = _db.get_index< volatile_comment_index, by_permlink >();
  auto _vfound = _volatile_idx.find( _found->get_author_and_permlink_hash() );

  if( _vfound != _volatile_idx.end() )
    return;

  if( dbg_info )
  {
    ilog( "rocksdb_storage_provider: Store a comment with hash: ${hash}, with permlink/author: ${permlink}/${author}",
    ("hash", comment_object::compute_author_and_permlink_hash( _account.get_id(), op.permlink ))
    ("permlink", op.permlink)("author", op.author) );
  }

  _db.create< volatile_comment_object >( [&]( volatile_comment_object& o )
  {
    o.comment_id = _found->get_id();
    o.block_number = _db.head_block_num();
    o.set_author_and_permlink_hash( _found->get_author_and_permlink_hash() );
  });
}

void rocksdb_storage_provider::comment_was_paid( const account_id_type& account_id, const shared_string& permlink )
{
  auto& _db = mgr->get_database();

  auto& _account = _db.get_account( account_id );

  if( dbg_info )
  {
    ilog("rocksdb_storage_provider: A comment with hash: ${hash} permlink/author: ${permlink}/${author} was paid.",
          ("hash", comment_object::compute_author_and_permlink_hash( account_id, permlink.c_str()))
          ("permlink", permlink)("author", _account.get_name()) );
  }

  const auto& _volatile_idx = _db.get_index< volatile_comment_index, by_permlink >();
  auto _found = _volatile_idx.find( comment_object::compute_author_and_permlink_hash( account_id, permlink.c_str()) );
  FC_ASSERT( _found != _volatile_idx.end() );

  const auto& _comment = _db.get_comment( _found->comment_id );

  _db.modify( *_found, [&_comment]( volatile_comment_object& vc )
  {
    vc.parent_comment           = _comment.get_parent_id();
    vc.depth                    = _comment.get_depth(); 

    vc.was_paid                 = true;
  });
}

void rocksdb_storage_provider::move_to_external_storage_impl( const volatile_comment_object& volatile_object )
{
  // if( !volatile_object.was_paid )
  //   return;

  if( dbg_info )
  {
    ilog( "rocksdb_storage_provider: Move to external storage a comment with id: ${comment_id}, hash: ${hash}",
          ("comment_id", volatile_object.comment_id)("hash", volatile_object.get_author_and_permlink_hash()) );
  }

  Slice _key_slice( volatile_object.get_author_and_permlink_hash().data(), volatile_object.get_author_and_permlink_hash().data_size() );

  rocksdb_comment_object _obj( volatile_object );

  auto _serialize_buffer = dump( _obj );
  Slice _value_slice( _serialize_buffer.data(), _serialize_buffer.size() );

  mgr->save( _key_slice, _value_slice, 0/*column_number*/ );
}

void rocksdb_storage_provider::move_to_external_storage( uint32_t block_num )
{
  auto& _db = mgr->get_database();

  const auto& _volatile_idx = _db.get_index< volatile_comment_index, by_block >();

  auto _it = _volatile_idx.find( block_num );

  while( _it != _volatile_idx.end() && _it->block_number <= block_num )
  {
    move_to_external_storage_impl( *_it );

    //temporary disabled!!!!
    //const auto& _comment = db.get_comment( _it->comment_id );
    //db.remove( _comment );

    ++_it;
  }
}

std::shared_ptr<comment_object> rocksdb_storage_provider::find_comment( const account_id_type& author, const std::string& permlink )
{
  auto _hash = comment_object::compute_author_and_permlink_hash( author, permlink );

  Slice _key( _hash.data(), _hash.data_size() );

  std::string _buffer;

  mgr->read( _key, _buffer, 0/*column_number*/ );

  rocksdb_comment_object _obj;

  load( _obj, _buffer.data(), _buffer.size() );

  return std::shared_ptr<comment_object>( new comment_object ( _obj.id,
                                                  oid_ref< comment_object>( oid<comment_object>( _obj.parent_comment ) ),
                                                   _hash, _obj.depth ) );
}

}}
