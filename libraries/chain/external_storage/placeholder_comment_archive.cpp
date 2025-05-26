#include <hive/chain/database.hpp>

#include <hive/chain/external_storage/placeholder_comment_archive.hpp>

namespace hive { namespace chain {

hive::utilities::benchmark_dumper::comment_archive_details_t comments_handler::stats;

void placeholder_comment_archive::on_cashout( uint32_t _block_num, const comment_object& _comment, const comment_cashout_object& _comment_cashout )
{
  ++stats.comment_cashout_processing.count;
  // just count calls
}

void placeholder_comment_archive::on_irreversible_block( uint32_t block_num )
{
  // do nothing - we are not processing any comments in this call
}

comment placeholder_comment_archive::get_comment( const account_id_type& author, const std::string& permlink, bool comment_is_required ) const
{
  auto time_start = std::chrono::high_resolution_clock::now();

  auto _hash = comment_object::compute_author_and_permlink_hash( author, permlink );
  const auto* _comment = db.find< comment_object, by_permlink >( _hash );
  if( _comment )
  {
    stats.comment_accessed_from_index.time_ns += std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
    ++stats.comment_accessed_from_index.count;
    return comment( _comment );
  }
  else
  {
    stats.comment_not_found.time_ns += std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
    ++stats.comment_not_found.count;
    FC_ASSERT( not comment_is_required, "Comment with `id`/`permlink` ${author}/${permlink} not found", ( author ) ( permlink ) );
    return comment();
  }
}

void placeholder_comment_archive::save_snapshot( const prepare_snapshot_supplement_notification& note )
{
  // do nothing - there is no extra data to store
}

void placeholder_comment_archive::load_snapshot( const load_snapshot_supplement_notification& note )
{
  // do nothing - there is no extra data to load
}

void placeholder_comment_archive::open()
{
  // do nothing - there is no extra data
}

void placeholder_comment_archive::close()
{
  // do nothing - there is no extra data
}

void placeholder_comment_archive::wipe()
{
  // do nothing - there is no extra data
}

} } // hive::chain

