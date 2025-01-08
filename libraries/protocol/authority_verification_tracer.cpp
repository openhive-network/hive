#include <hive/protocol/authority_verification_tracer.hpp>

namespace hive { namespace protocol {

bool authority_verification_tracer::detect_cycle(std::string account) const
{
  for( const path_entry& pe : _current_authority_path )
  {
    if( pe.processed_entry == account )
      return true;
  }

  return false;
}

authority_verification_trace::path_entry& authority_verification_tracer::get_parent_entry()
{
  FC_ASSERT(not _current_authority_path.empty());
  return _current_authority_path.back();
}

void authority_verification_tracer::push_parent_entry(const path_entry& entry)
{
  _current_authority_path.push_back(entry);
}

void authority_verification_tracer::pop_parent_entry()
{
  FC_ASSERT(not _current_authority_path.empty());
  _current_authority_path.pop_back();
}

void authority_verification_tracer::push_final_path_entry(const path_entry& entry)
{
  _trace.final_authority_path.push_back(entry);
}

void authority_verification_tracer::pop_final_path_entry()
{
  FC_ASSERT(not _trace.final_authority_path.empty());
  _trace.final_authority_path.pop_back();
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

  _trace.root = root_path_entry;
  push_parent_entry(root_path_entry);
  push_final_path_entry(root_path_entry);
}

void authority_verification_tracer::on_root_authority_finish( unsigned int verification_status )
{
  _trace.verification_status = verification_status;
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
  
  if (parent_threshold_reached)
    get_parent_entry().flags &= ~INSUFFICIENT_WEIGHT;

  get_parent_entry().visited_entries.push_back(key_entry);
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
  unsigned int weight, unsigned int parent_threshold, unsigned int parent_depth )
{
  path_entry entry{
    processed_entry: account,
    processed_role: _current_role,
    recursion_depth: parent_depth + 1,
    threshold: parent_threshold,
    weight: weight,
    flags: INSUFFICIENT_WEIGHT
  };

  if(detect_cycle(account))
    entry.flags |= CYCLE_DETECTED;

  get_parent_entry().visited_entries.push_back(entry);
  push_final_path_entry(entry);
  push_parent_entry(entry);
}

void authority_verification_tracer::on_leaving_account_entry( bool is_last_account_auth, bool parent_threshold_reached )
{
  if( parent_threshold_reached )
    get_parent_entry().flags &= ~INSUFFICIENT_WEIGHT;

  if( not is_last_account_auth )
    pop_final_path_entry(); // drop its path_entry from last path
  
  pop_parent_entry();
}

} } // hive::protocol
