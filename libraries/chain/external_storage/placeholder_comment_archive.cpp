#include <hive/chain/database.hpp>

#include <hive/chain/external_storage/placeholder_comment_archive.hpp>

namespace hive { namespace chain {

void placeholder_comment_archive::on_cashout( const comment_object& _comment, const comment_cashout_object& _comment_cashout )
{
  // do nothing
}

void placeholder_comment_archive::on_irreversible_block( uint32_t block_num )
{
  // do nothing
}

comment placeholder_comment_archive::get_comment( const account_id_type& author, const std::string& permlink, bool comment_is_required ) const
{
  auto _hash = comment_object::compute_author_and_permlink_hash( author, permlink );
  const auto* _comment = db.find< comment_object, by_permlink >( _hash );
  if( _comment )
  {
    return comment( _comment );
  }
  else
  {
    FC_ASSERT( !comment_is_required, "Comment with `id`/`permlink` ${author}/${permlink} not found", ( author ) ( permlink ) );
    return comment();
  }
}

void placeholder_comment_archive::save_snaphot( const prepare_snapshot_supplement_notification& note )
{
  // do nothing
}

void placeholder_comment_archive::load_snapshot( const load_snapshot_supplement_notification& note )
{
  // do nothing
}

void placeholder_comment_archive::open()
{
  // do nothing
}

void placeholder_comment_archive::close()
{
  // do nothing
}

void placeholder_comment_archive::wipe()
{
  // do nothing
}

void placeholder_comment_archive::update_lib( uint32_t lib )
{
  // do nothing
}

void placeholder_comment_archive::update_reindex_point( uint32_t rp )
{
  // do nothing
}

} } // hive::chain

