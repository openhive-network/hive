#include <hive/chain/database.hpp>

#include <hive/chain/external_storage/memory_comment_archive.hpp>

namespace hive { namespace chain {

void memory_comment_archive::on_cashout( uint32_t _block_num, const comment_object& _comment, const comment_cashout_object& _comment_cashout )
{
  auto time_start = std::chrono::high_resolution_clock::now();

  comments_in_blocks[ _block_num ].emplace_back( _comment.get_id() );
  // note that even in very rare case of cashout event being reverted and cashout being reapplied or comment deleted (which means
  // we have either duplicate or dead entries), it doesn't matter as long as we don't expect to find comment during LIB processing

  stats.comment_cashout_processing.time_ns += std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
  ++stats.comment_cashout_processing.count;
}

void memory_comment_archive::on_irreversible_block( uint32_t block_num )
{
  archive->last_irreversible_block_num = block_num;

  if( !db.has_hardfork( HIVE_HARDFORK_0_19 ) )
    return;

  auto time_start = std::chrono::high_resolution_clock::now();

  const auto& comment_idx = db.get_index< comment_index, by_id >();

  uint64_t count = 0;
  auto blockI = comments_in_blocks.begin();
  while( blockI != comments_in_blocks.end() && blockI->first <= block_num )
  {
    const auto& comment_ids = blockI->second;
    ++blockI;
    for( auto comment_id : comment_ids )
    {
      auto commentI = comment_idx.find( comment_id );
      if( commentI == comment_idx.end() )
        continue; // see comment in on_cashout

      archive->comments.emplace( commentI->copy_chain_object() );
      db.remove_no_undo( *commentI );
      ++count;
    }
    comments_in_blocks.erase( comments_in_blocks.begin() );
  }

  stats.comment_lib_processing.time_ns += std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
  stats.comment_lib_processing.count += count;
}

comment memory_comment_archive::get_comment( const account_id_type& author, const std::string& permlink, bool comment_is_required ) const
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
  
  auto commentI = archive->comments.find( _hash );
  if( commentI != archive->comments.end() )
  {
    stats.comment_accessed_from_archive.time_ns += std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
    ++stats.comment_accessed_from_archive.count;
    return comment( &*commentI );
  }
  else
  {
    stats.comment_not_found.time_ns += std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
    ++stats.comment_not_found.count;
    FC_ASSERT( !comment_is_required, "Comment with `id`/`permlink` ${author}/${permlink} not found", ( author ) ( permlink ) );
    return comment();
  }
}

void memory_comment_archive::save_snaphot( const prepare_snapshot_supplement_notification& note )
{
  // TODO: supplement
  FC_ASSERT( false, "Not implemented yet" );
}

void memory_comment_archive::load_snapshot( const load_snapshot_supplement_notification& note )
{
  // TODO: supplement
  FC_ASSERT( false, "Not implemented yet" );
}

void memory_comment_archive::open()
{
  // create or find stored LIB and archive set
  auto s = db.get_segment_manager();
  archive = s->find_or_construct<archive_object_type>( "memory_comment_archive" )( allocator< archive_object_type >( s ) );
  FC_ASSERT( db.get_last_irreversible_block_num() == archive->last_irreversible_block_num, "Data range mismatch between database and comment archive" );
    // did you change comment-archive config option? if so, you need to replay
}

void memory_comment_archive::close()
{
  // since storage is part of SHM, database::close is enough, nothing extra to do here
}

void memory_comment_archive::wipe()
{
  // since storage is part of SHM, database::wipe clears it, nothing extra to do here
}

} } // hive::chain

