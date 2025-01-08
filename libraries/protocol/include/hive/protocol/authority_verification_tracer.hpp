#pragma once

#include <hive/protocol/authority_trace_data.hpp>
#include <hive/protocol/types.hpp>

#include <string>
#include <vector>

namespace hive { namespace protocol {

class authority_verification_tracer
{
  public:
    using path_entry = authority_verification_trace::path_entry;

    void set_role(std::string role) { _current_role = role; }
    authority_verification_trace get_trace() { return _trace; }

    void on_root_authority_start( const account_name_type& account, unsigned int threshold,
                                  unsigned int depth );
    void on_matching_key( const public_key_type& key, unsigned int weight,
                          unsigned int parent_threshold, unsigned int depth,
                          bool parent_threshold_reached );
    void on_missing_matching_key();
    void on_unknown_account_entry( const account_name_type& account, unsigned int weight,
                                   unsigned int parent_threshold, unsigned int parent_depth);
    /// called only if account is known
    void on_entering_account_entry( const account_name_type& account, unsigned int weight,
                                    unsigned int parent_threshold, unsigned int parent_depth );
    void on_account_processing_limit_exceeded();
    void on_recursion_depth_limit_exceeded();
    /// called only if account is known
    void on_leaving_account_entry( bool is_last_account_auth, bool parent_threshold_reached );
    void on_root_authority_finish( unsigned int verification_status );

  private:
    bool detect_cycle(std::string account) const;
    path_entry& get_parent_entry();
    void push_parent_entry(const path_entry& entry);
    void pop_parent_entry();
    void push_final_path_entry(const path_entry& entry);
    void pop_final_path_entry();

  private:
    authority_verification_trace _trace;
    std::string                  _current_role;
    std::vector<path_entry>      _current_authority_path;
};

} } // hive::protocol

