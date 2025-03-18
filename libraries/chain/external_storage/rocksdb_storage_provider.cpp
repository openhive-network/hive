
#include <hive/chain/hive_object_types.hpp>
#include <hive/chain/external_storage/rocksdb_storage_provider.hpp>
#include <hive/chain/external_storage/utilities.hpp>

namespace hive { namespace chain {

void rocksdb_storage_provider::store_comment( const comment_id_type& comment_id, uint32_t block_number )
{
  auto& _db = mgr->get_database();

  _db.create< volatile_comment_object >( [&]( volatile_comment_object& o )
  {
    o.comment_id = comment_id;
    o.block_number = block_number;
  });
}

void rocksdb_storage_provider::comment_was_paid( const comment_cashout_object& comment_cashout )
{
  auto& _db = mgr->get_database();

  auto& _account = _db.get_account( comment_cashout.get_author_id() );

  ilog("A comment with id: ${id}, hash: ${hash} permlink/author: ${permlink}/${author} was paid.",
        ("id", comment_cashout.get_comment_id())("hash", comment_object::compute_author_and_permlink_hash(comment_cashout.get_author_id(), comment_cashout.get_permlink().c_str()))
        ("permlink", comment_cashout.get_permlink())("author", _account.get_name()) );

  const auto& _volatile_idx = _db.get_index< volatile_comment_index, by_comment_id >();
  auto _found = _volatile_idx.find( comment_cashout.get_comment_id() );
  FC_ASSERT( _found != _volatile_idx.end() );

  const auto& _comment = _db.get_comment( comment_cashout.get_comment_id() );

  _db.modify( *_found, [&_comment]( volatile_comment_object& vc )
  {
    vc.parent_comment           = _comment.get_parent_id();
    vc.author_and_permlink_hash = _comment.get_author_and_permlink_hash();
    vc.depth                    = _comment.get_depth(); 

    vc.was_paid                 = true;
  });
}

void rocksdb_storage_provider::move_to_external_storage( const volatile_comment_object& volatile_object )
{
  Slice _key_slice( volatile_object.author_and_permlink_hash.data(), volatile_object.author_and_permlink_hash.data_size() );

  rocksdb_comment_object _obj( volatile_object );

  auto _serialize_buffer = dump( _obj );
  Slice _value_slice( _serialize_buffer.data(), _serialize_buffer.size() );

  mgr->save( _key_slice, _value_slice, 0/*column_number*/ );
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
