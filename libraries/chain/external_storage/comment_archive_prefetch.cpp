#include <hive/chain/external_storage/comment_archive_prefetch.hpp>

#include <hive/protocol/config.hpp>

#include <algorithm>

namespace hive { namespace chain {

namespace {

using namespace hive::protocol;

/// Mirrors the lookups by author and permlink in the evaluators (hive_evaluator_social.cpp, dhf_evaluator.cpp).
struct comment_lookup_key_collector
{
  typedef void result_type;

  std::vector<comment_lookup_key>& keys;

  void add( const account_name_type& author, const std::string& permlink ) const
  {
    keys.push_back( comment_lookup_key{ author, permlink } );
  }

  void operator()( const vote_operation& op ) const
  {
    add( op.author, op.permlink );
  }

  void operator()( const comment_operation& op ) const
  {
    add( op.author, op.permlink );
    if( op.parent_author != HIVE_ROOT_POST_PARENT )
      add( op.parent_author, op.parent_permlink );
  }

  void operator()( const comment_options_operation& op ) const
  {
    add( op.author, op.permlink );
  }

  void operator()( const delete_comment_operation& op ) const
  {
    add( op.author, op.permlink );
  }

  void operator()( const create_proposal_operation& op ) const
  {
    add( op.creator, op.permlink );
    add( op.receiver, op.permlink );
  }

  void operator()( const update_proposal_operation& op ) const
  {
    // the evaluator requires the creator of the operation to be the creator of the proposal
    add( op.creator, op.permlink );
  }

  template< typename T >
  void operator()( const T& ) const {}
};

using lookup_path = hive::utilities::benchmark_dumper::comment_lookup_path;

struct evaluator_lookup_path_visitor
{
  typedef lookup_path result_type;

  lookup_path operator()( const vote_operation& ) const { return lookup_path::vote_evaluator; }
  lookup_path operator()( const comment_operation& ) const { return lookup_path::comment_evaluator; }
  lookup_path operator()( const comment_options_operation& ) const { return lookup_path::comment_options_evaluator; }
  lookup_path operator()( const delete_comment_operation& ) const { return lookup_path::delete_comment_evaluator; }
  lookup_path operator()( const create_proposal_operation& ) const { return lookup_path::proposal_evaluator; }
  lookup_path operator()( const update_proposal_operation& ) const { return lookup_path::proposal_evaluator; }

  template< typename T >
  lookup_path operator()( const T& ) const { return lookup_path::other_evaluator; }
};

}

void collect_comment_lookup_keys( const hive::protocol::signed_transaction& transaction, std::vector<comment_lookup_key>& keys )
{
  comment_lookup_key_collector collector{ keys };
  for( const auto& op : transaction.operations )
    op.visit( collector );
}

hive::utilities::benchmark_dumper::comment_lookup_path get_evaluator_lookup_path( const hive::protocol::operation& op )
{
  return op.visit( evaluator_lookup_path_visitor() );
}

prefetched_comment_cache::prefetched_comment_cache( size_t capacity ) : capacity( capacity )
{
}

void prefetched_comment_cache::refresh( const hash_type& hash, slot& s )
{
  s.serial = next_serial++;
  order.emplace_back( hash, s.serial );
}

void prefetched_comment_cache::evict()
{
  // stale items in `order` are dropped too, so it stays within twice the capacity
  while( !order.empty() && ( entries.size() > capacity || order.size() > 2 * capacity ) )
  {
    auto itr = entries.find( order.front().first );
    if( itr != entries.end() && itr->second.serial == order.front().second )
      entries.erase( itr );
    order.pop_front();
  }
}

void prefetched_comment_cache::keep_missing( std::vector<hash_type>& hashes )
{
  std::lock_guard<std::mutex> lock( mutex );
  auto end = std::remove_if( hashes.begin(), hashes.end(), [this]( const hash_type& hash )
  {
    auto itr = entries.find( hash );
    if( itr == entries.end() )
      return false;
    refresh( hash, itr->second );
    return true;
  } );
  hashes.erase( end, hashes.end() );
  evict();
}

void prefetched_comment_cache::store( const std::vector<std::pair<hash_type, rocksdb_comment_object>>& new_entries )
{
  std::lock_guard<std::mutex> lock( mutex );
  for( const auto& [ hash, entry ] : new_entries )
  {
    auto& s = entries[ hash ];
    s.entry = entry;
    refresh( hash, s );
  }
  evict();
}

std::optional<rocksdb_comment_object> prefetched_comment_cache::find( const hash_type& hash ) const
{
  std::lock_guard<std::mutex> lock( mutex );
  auto itr = entries.find( hash );
  if( itr == entries.end() )
    return {};
  return itr->second.entry;
}

void prefetched_comment_cache::clear()
{
  std::lock_guard<std::mutex> lock( mutex );
  entries.clear();
  order.clear();
}

author_id_cache::author_id_cache( size_t capacity ) : capacity( capacity )
{
}

void author_id_cache::remember( const account_id_type& author, const std::function<account_name_type()>& get_name )
{
  const size_t index = author.get_value();
  std::lock_guard<std::mutex> lock( mutex );
  if( index < seen.size() && seen[ index ] )
    return;
  if( ids.size() >= capacity )
  {
    ids.clear();
    seen.clear();
  }
  ids.insert_or_assign( get_name(), author );
  if( index >= seen.size() )
    seen.resize( index + 1 );
  seen[ index ] = true;
}

std::optional<account_id_type> author_id_cache::find( const account_name_type& name ) const
{
  std::lock_guard<std::mutex> lock( mutex );
  auto itr = ids.find( name );
  if( itr == ids.end() )
    return {};
  return itr->second;
}

void author_id_cache::clear()
{
  std::lock_guard<std::mutex> lock( mutex );
  ids.clear();
  seen.clear();
}

} } // hive::chain
