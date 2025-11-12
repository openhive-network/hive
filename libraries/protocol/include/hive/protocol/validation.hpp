
#pragma once

#include <hive/protocol/hive_specialised_exceptions.hpp>

#include <hive/protocol/asset.hpp>

#include <hive/protocol/base.hpp>
#include <hive/protocol/block_header.hpp>
#include <hive/protocol/config.hpp>

#include <fc/utf8.hpp>

namespace hive { namespace protocol {

class NoExtraObjectProvided{};

inline bool is_asset_type( const asset& asset, asset_symbol_type symbol )
{
  return asset.symbol == symbol;
}

template<typename ExtraContextObjectT = NoExtraObjectProvided>
inline void validate_asset_type( const asset& asset, asset_symbol_type symbol, const char* context = "", const ExtraContextObjectT& extra_context_obj = ExtraContextObjectT{} )
{
  HIVE_SPECIALISED_ASSERT_WITH_SUBJECT( "protocol", "asset", asset, is_asset_type(asset, symbol),
    "Asset symbol does not match expected symbol. Expected: ${expected}, Actual: ${subject}",
    ("expected", symbol)("context", context)(extra_context_obj) );
}

template<typename ExtraContextObjectT = NoExtraObjectProvided>
inline void validate_asset_type_one_of( const asset& asset, const std::set<asset_symbol_type>& symbols, const char* context = "", const ExtraContextObjectT& extra_context_obj = ExtraContextObjectT{} )
{
  HIVE_SPECIALISED_ASSERT_WITH_SUBJECT( "protocol", "asset", asset, symbols.find(asset.symbol) != symbols.end(),
    "Asset symbol does not match expected symbol. Expected: ${expected}, Actual: ${subject}",
    ("expected", symbols)("context", context)(extra_context_obj) );
}

template<typename ExtraContextObjectT = NoExtraObjectProvided>
inline void validate_asset_greater_than_zero( const asset& asset, const char* context = "", const ExtraContextObjectT& extra_context_obj = ExtraContextObjectT{} )
{
  HIVE_SPECIALISED_ASSERT_WITH_SUBJECT( "protocol", "asset", asset, asset.amount > 0,
    "Asset amount must be greater than zero. Actual: ${subject}",
    ("context", context)(extra_context_obj) );
}

template<typename ExtraContextObjectT = NoExtraObjectProvided>
inline void validate_asset_not_negative( const asset& asset, const char* context = "", const ExtraContextObjectT& extra_context_obj = ExtraContextObjectT{} )
{
  HIVE_SPECIALISED_ASSERT_WITH_SUBJECT( "protocol", "asset", asset, asset.amount >= 0,
    "Asset amount cannot be negative. Actual: ${subject}",
    ("context", context)(extra_context_obj) );
}

template<typename ExtraContextObjectT = NoExtraObjectProvided>
inline void validate_asset_is_not_vesting( const asset& asset, const char* context = "", const ExtraContextObjectT& extra_context_obj = ExtraContextObjectT{} )
{
  HIVE_SPECIALISED_ASSERT_WITH_SUBJECT( "protocol", "asset", asset, not asset.symbol.is_vesting(),
    "Asset cannot be of VESTS type. Actual: ${subject}",
    ("context", context)(extra_context_obj) );
}

template<typename ExtraContextObjectT = NoExtraObjectProvided>
inline void validate_string_max_size(const fc::string& str, const size_t max_size, const char* context = "", const ExtraContextObjectT& extra_context_obj = ExtraContextObjectT{} )
{
  HIVE_SPECIALISED_ASSERT_WITH_SUBJECT( "protocol", "string", str, str.size() < max_size,
    "String is too large. Size: ${size}, Max: ${max}",
    ("size", str.size())("max", max_size)("context", context)(extra_context_obj) );
}

template<typename ExtraContextObjectT = NoExtraObjectProvided>
inline void validate_is_string_in_utf8(const fc::string& str, const char* context = "", const ExtraContextObjectT& extra_context_obj = ExtraContextObjectT{} )
{
  HIVE_SPECIALISED_ASSERT_WITH_SUBJECT( "protocol", "string", str, fc::is_utf8( str ),
    "String is not valid UTF8.",
    ("context", context)(extra_context_obj) );
}

template<typename ExtraContextObjectT = NoExtraObjectProvided>
inline void validate_string_is_not_empty(const fc::string& str, const char* context = "", const ExtraContextObjectT& extra_context_obj = ExtraContextObjectT{} )
{
  HIVE_SPECIALISED_ASSERT_WITH_SUBJECT( "protocol", "string", str, str.size() > 0,
    "String is empty.",
    ("context", context)(extra_context_obj) );
}

template<typename int_t, typename ExtraContextObjectT = NoExtraObjectProvided>
inline void validate_number_in_100_percent_range(const int_t number, const char* context = "", const ExtraContextObjectT& extra_context_obj = ExtraContextObjectT{} )
{
  HIVE_SPECIALISED_ASSERT_WITH_SUBJECT( "protocol", "number", number, number <= HIVE_100_PERCENT,
    "Number exceeds 100 percent. Value: ${subject}, Max: ${max}",
    ("context", context)("max", HIVE_100_PERCENT)(extra_context_obj) );
}

inline void validate_account_name( const string& name )
{
  account_name_validity validity_check_result = detail::check_account_name( name );

  HIVE_PROTOCOL_ACCOUNT_NAME_ASSERT( validity_check_result != account_name_validity::too_short,
    "Account name '${subject}' is too short. Use at least ${min} characters.",
    ("subject", name)("reason", validity_check_result)("min", HIVE_MIN_ACCOUNT_NAME_LENGTH) );

  HIVE_PROTOCOL_ACCOUNT_NAME_ASSERT( validity_check_result != account_name_validity::too_long,
    "Account name '${subject}' is too long. Use maximum of ${max} characters.",
    ("subject", name)("reason", validity_check_result)("max", HIVE_MAX_ACCOUNT_NAME_LENGTH) );

  HIVE_PROTOCOL_ACCOUNT_NAME_ASSERT( validity_check_result != account_name_validity::invalid_sequence,
    "Account name '${subject}' is not valid. Please follow the RFC 1035 rules.", ("subject", name)("reason", validity_check_result) );
}

inline void validate_permlink( const string& permlink )
{
  HIVE_PROTOCOL_PERMLINK_ASSERT( permlink.size() < HIVE_MAX_PERMLINK_LENGTH, "permlink is too long: `${subject}`", ("subject", permlink) );
  HIVE_PROTOCOL_PERMLINK_ASSERT( fc::is_utf8( permlink ), "permlink not formatted in UTF8", ("subject", permlink) );
}

} }

FC_REFLECT_EMPTY(hive::protocol::NoExtraObjectProvided);
