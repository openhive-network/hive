#include <hive/protocol/authority_verification_tracer.hpp>

namespace hive { namespace protocol {

bool authority_verification_tracer::detect_cycle(std::string account) const
{
  const path_entry* entry = &( get_root_entry() );
  if( entry->processed_entry == account )
    return true;

  for( size_t index : _current_authority_path )
  {
    entry = &( entry->visited_entries.at( index ) );
    if( entry->processed_entry == account )
      return true;
  }

  return false;
}

authority_verification_trace::path_entry& authority_verification_tracer::get_root_entry()
{
  FC_ASSERT( not _trace.root.empty() );
  return _trace.root.back();
}

const authority_verification_trace::path_entry& authority_verification_tracer::get_root_entry() const
{
  FC_ASSERT( not _trace.root.empty() );
  return _trace.root.back();
}

authority_verification_trace::path_entry& authority_verification_tracer::get_parent_entry()
{
  path_entry* parent_entry = &( get_root_entry() );
  for( size_t index : _current_authority_path )
    parent_entry = &( parent_entry->visited_entries.at( index ) );

  return *parent_entry;
}

void authority_verification_tracer::push_parent_entry()
{
  path_entry& parent_entry = get_parent_entry();
  size_t aux = parent_entry.visited_entries.size();
  FC_ASSERT(aux > 0, "Push parent entry AFTER putting it into visited_entries!");
  _current_authority_path.push_back( aux -1 );
}

void authority_verification_tracer::pop_parent_entry()
{
  FC_ASSERT(not _current_authority_path.empty());
  _current_authority_path.pop_back();
}

void authority_verification_tracer::copy_successful_offsprings( const path_entry& from,
  path_entry& to )
{
  for( const path_entry& child : from.visited_entries )
  {
    if( ( child.flags & MATCHING_KEY ) == 0 &&
        ( child.weight < child.threshold ) )
      continue;

    to.visited_entries.push_back({
      processed_entry: child.processed_entry,
      processed_role: child.processed_role,
      recursion_depth: child.recursion_depth,
      threshold: child.threshold,
      weight: child.weight,
      flags: child.flags
    });

    path_entry& child_copy = to.visited_entries.back();
    copy_successful_offsprings( child, child_copy );
  }
}

void authority_verification_tracer::fill_final_authority_path()
{
  const path_entry& root_entry = get_root_entry();
  path_entry childless_root_entry {
    processed_entry: root_entry.processed_entry,
    processed_role: root_entry.processed_role,
    recursion_depth: root_entry.recursion_depth,
    threshold: root_entry.threshold,
    weight: root_entry.weight,
    flags: root_entry.flags
  };
  _trace.final_authority_path.push_back( childless_root_entry );

  // For failed attempt copy only root entry without its offsprings, as
  // they are available in original root entry for detailed analysis if needed.
  if( root_entry.weight < root_entry.threshold )
    return;

  // For successful attempt copy only successful offsprings.
  copy_successful_offsprings( root_entry, _trace.final_authority_path.back() );
}

void authority_verification_tracer::on_root_authority_start( const account_name_type& account,
  unsigned int threshold, unsigned int depth )
{
  path_entry root_path_entry {
    processed_entry: account,
    processed_role: _current_role,
    recursion_depth: depth,
    threshold: threshold,
    flags: INSUFFICIENT_WEIGHT
  };

  _trace.root.push_back( root_path_entry );
}

void authority_verification_tracer::on_root_authority_finish( bool success,
  unsigned int verification_status )
{
  if( success )
  {
    get_parent_entry().flags &= ~INSUFFICIENT_WEIGHT;
  }

  _trace.verification_status = verification_status;
  fill_final_authority_path();
}

void authority_verification_tracer::on_empty_auth()
{
  get_parent_entry().flags |= EMPTY_AUTHORITY;
}

void authority_verification_tracer::on_approved_authority( const account_name_type& account,
  unsigned int weight )
{
  path_entry& parent = get_parent_entry();
  parent.weight += weight;
  if( parent.processed_entry == account )
  {
    parent.flags &= ~INSUFFICIENT_WEIGHT;
    parent.flags |= RESOLVED_BY_APPROVAL;
    _trace.final_authority_path.push_back( get_root_entry() );
  }
  else
  {
    path_entry entry{
      processed_entry: account,
      processed_role: _current_role,
      recursion_depth: parent.recursion_depth + 1,
      threshold: parent.threshold,
      weight: weight,
      flags: RESOLVED_BY_APPROVAL
    };

    parent.visited_entries.push_back( entry );
  }
}

void authority_verification_tracer::on_matching_key( const public_key_type& key,
  unsigned int weight, unsigned int parent_threshold, unsigned int depth,
  bool parent_threshold_reached )
{
  path_entry key_entry {
    processed_entry: (std::string)key,
    processed_role: _current_role,
    recursion_depth: depth,
    threshold: parent_threshold,
    weight: weight,
    flags: MATCHING_KEY
  };

  if (weight < parent_threshold)
    key_entry.flags |= INSUFFICIENT_WEIGHT;
  
  path_entry& parent_entry = get_parent_entry();

  parent_entry.weight += weight;

  if (parent_threshold_reached)
    parent_entry.flags &= ~INSUFFICIENT_WEIGHT;

  parent_entry.visited_entries.push_back(key_entry);
}

void authority_verification_tracer::on_missing_matching_key()
{
  get_parent_entry().flags |= NOT_MATCHING_KEY;
}

void authority_verification_tracer::on_account_processing_limit_exceeded()
{
  get_parent_entry().flags |= ACCOUNT_LIMIT_EXCEEDED;
}

void authority_verification_tracer::on_recursion_depth_limit_exceeded()
{
  get_parent_entry().flags |= DEPTH_LIMIT_EXCEEDED;
}

void authority_verification_tracer::on_unknown_account_entry( const account_name_type& account,
  unsigned int weight, unsigned int parent_threshold, unsigned int parent_depth )
{
  path_entry entry{
    processed_entry: account,
    processed_role: _current_role,
    recursion_depth: parent_depth,
    threshold: parent_threshold,
    weight: weight,
    flags: MISSING_ACCOUNT
  };

  get_parent_entry().visited_entries.push_back(entry);
}

void authority_verification_tracer::on_entering_account_entry( const account_name_type& account,
  unsigned int weight, unsigned int account_threshold, unsigned int parent_depth )
{
  path_entry entry{
    processed_entry: account,
    processed_role: _current_role,
    recursion_depth: parent_depth + 1,
    threshold: account_threshold,
    flags: INSUFFICIENT_WEIGHT
  };

  if(detect_cycle(account))
    entry.flags |= CYCLE_DETECTED;

  get_parent_entry().visited_entries.push_back(entry);
  push_parent_entry();
}

void authority_verification_tracer::on_leaving_account_entry( unsigned int effective_weight,
  bool parent_threshold_reached )
{
  if( parent_threshold_reached )
    get_parent_entry().flags &= ~INSUFFICIENT_WEIGHT;

  pop_parent_entry();

  if( effective_weight > 0)
  {
    path_entry& parent_entry = get_parent_entry();
    parent_entry.weight += effective_weight;
  }
}

void authority_verification_tracer::trim_final_authority_path()
{
  FC_ASSERT(not _trace.final_authority_path.empty());
  _trace.final_authority_path.pop_back();
}

} } // hive::protocol
