
#include <hive/chain/detail/state/account_object.hpp>

#include <hive/chain/block_summary_object_multiindex.hpp>
#include <hive/chain/comment_object_multiindex.hpp>

#include <hive/chain/index.hpp>
#include <chainbase/chainbase.inl>

#include <hive/chain/util/type_registrar_definition.hpp>

namespace hive { namespace chain {

void initialize_core_indexes_03( database& db )
{
  HIVE_ADD_CORE_INDEX(db, block_summary_index);
  HIVE_ADD_CORE_INDEX(db, comment_index);
  HIVE_ADD_CORE_INDEX(db, comment_vote_index);  
}

const comment_object* database::find_comment( comment_id_type comment_id )const
{
  return find< comment_object, by_id >( comment_id );
}

const comment_object& database::get_comment_for_payout_time( const comment_object& comment )const
{
  if( has_hardfork( HIVE_HARDFORK_0_17__769 ) || comment.is_root() )
    return comment;
  else
    return get< comment_object >( find_comment_cashout_ex( comment )->get_root_id() );
}

const comment_cashout_object* database::find_comment_cashout( comment_id_type comment_id ) const
{
  const auto& idx = get_index< comment_cashout_index, by_id >();
  comment_cashout_object::id_type ccid( comment_id );
  auto found = idx.find( ccid );

  if( found == idx.end() )
    return nullptr;
  else
    return &( *found );
}

const comment_cashout_ex_object* database::find_comment_cashout_ex( comment_id_type comment_id ) const
{
  if( has_hardfork( HIVE_HARDFORK_0_19 ) )
    return nullptr;

  const auto& idx = get_index< comment_cashout_ex_index, by_id >();
  comment_cashout_ex_object::id_type ccid( comment_id );
  auto found = idx.find( ccid );

  FC_ASSERT( found != idx.end() && "by id" );
  return &( *found );
}

const comment_object& database::get_comment( const comment_cashout_object& comment_cashout ) const
{
  const auto& idx = get_index< comment_index >().indices().get< by_id >();
  auto found = idx.find( comment_cashout.get_comment_id() );

  FC_ASSERT( found != idx.end() && "by cashout object" );

  return *found;
}

void database::remove_old_cashouts()
{
  // Remove all cashout extras
  auto& comment_cashout_ex_idx = get_mutable_index< comment_cashout_ex_index >();
  comment_cashout_ex_idx.clear();

  // Remove regular cashouts for paid comments
  const auto& idx = get_index< comment_cashout_index, by_cashout_time >();
  auto itr = idx.find( fc::time_point_sec::maximum() );

  auto block_num = head_block_num();
  while( itr != idx.end() )
  {
    const auto& current = *itr;
    const auto& comment = get_comment( current );
    ++itr;
    get_comments_handler().on_cashout( block_num, comment, current );
    remove( current );
  }
}

} }

HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::block_summary_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::comment_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::comment_vote_index)

// Explicit template instantiations for chainbase::database methods
template const chainbase::generic_index<hive::chain::block_summary_index>& chainbase::database::get_index<hive::chain::block_summary_index>() const;
template chainbase::generic_index<hive::chain::block_summary_index>& chainbase::database::get_mutable_index<hive::chain::block_summary_index>();

template const chainbase::generic_index<hive::chain::comment_index>& chainbase::database::get_index<hive::chain::comment_index>() const;
template chainbase::generic_index<hive::chain::comment_index>& chainbase::database::get_mutable_index<hive::chain::comment_index>();

template const chainbase::generic_index<hive::chain::comment_vote_index>& chainbase::database::get_index<hive::chain::comment_vote_index>() const;
template chainbase::generic_index<hive::chain::comment_vote_index>& chainbase::database::get_mutable_index<hive::chain::comment_vote_index>();
