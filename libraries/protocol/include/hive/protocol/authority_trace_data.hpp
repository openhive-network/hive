#pragma once

#include <string>
#include <vector>

namespace hive { namespace protocol {

enum authority_entry_processing_flags
{
  //ENTERING_AUTHORITY   = 0x001
  NOT_MATCHING_KEY        = 0x002,
  MATCHING_KEY            = 0x004,
  INSUFFICIENT_WEIGHT     = 0x008,
  DEPTH_LIMIT_EXCEEDED    = 0x010,
  ACCOUNT_LIMIT_EXCEEDED  = 0x020,
  CYCLE_DETECTED          = 0x040,
  MISSING_ACCOUNT         = 0x080,
  EMPTY_AUTHORITY         = 0X100,
};

/// @brief Contains detailed info of authority verification attempt.
struct authority_verification_trace
{
  /// @brief  Stores data on single authority entity (key or account).
  struct path_entry
  {
    std::string   processed_entry;      /// public key or account name
    std::string   processed_role;       /// authority level/role
    unsigned int  recursion_depth = 0;  /// how many account authorities we stepped down
    unsigned int  threshold = 0;        /// how much weight is needed on current depth
    unsigned int  weight = 0;           /// positive value on matched key/account
    unsigned int  flags = 0;            /// combination of authority_entry_processing_flags

    /// Stores the tree visited during processing of this authority entity (account actually).
    std::vector<path_entry> visited_entries;
  };

  /// Contains whole tree of visited paths.
  path_entry root;
  /// Contains the final sequence of entries, whether successful or not.
  std::vector<path_entry> final_authority_path;
  /** Combination of flags possibly indicating other problems detected by chain
   *  e.g. mixed authority levels, non-optimal signatures etc.
   */
  unsigned int verification_status;
};

} } // hive::protocol

