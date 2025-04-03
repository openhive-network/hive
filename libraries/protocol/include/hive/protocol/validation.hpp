
#pragma once

#include <hive/protocol/base.hpp>
#include <hive/protocol/block_header.hpp>
#include <hive/protocol/asset.hpp>
#include <hive/protocol/config.hpp>

#include <fc/utf8.hpp>

namespace hive { namespace protocol {

inline bool is_asset_type( asset asset, asset_symbol_type symbol )
{
  return asset.symbol == symbol;
}

inline void validate_account_name( const string& name )
{
  account_name_validity validity_check_result = detail::check_account_name( name );

  FC_ASSERT( validity_check_result != account_name_validity::too_short,
    "Account name '${name}' is too short. Use at least ${min} characters.",
    ("name", name)("min", HIVE_MIN_ACCOUNT_NAME_LENGTH) );

  FC_ASSERT( validity_check_result != account_name_validity::too_long,
    "Account name '${name}' is too long. Use maximum of ${max} characters.",
    ("name", name)("max", HIVE_MAX_ACCOUNT_NAME_LENGTH) );
  
  FC_ASSERT( validity_check_result != account_name_validity::invalid_sequence,
    "Account name '${name}' is not valid. Please follow the RFC 1035 rules.", ("name", name) );
}

inline void validate_permlink( const string& permlink )
{
  FC_ASSERT( permlink.size() < HIVE_MAX_PERMLINK_LENGTH, "permlink is too long" );
  FC_ASSERT( fc::is_utf8( permlink ), "permlink not formatted in UTF8" );
}

} }
