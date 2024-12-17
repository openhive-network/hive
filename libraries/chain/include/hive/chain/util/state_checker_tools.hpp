#pragma once

#include <hive/chain/util/decoded_types_data_storage.hpp>
#include <fc/variant_object.hpp>


namespace hive { namespace chain { namespace util {


// throws exception or log warning if doesn't match
void verify_match_of_state_definitions(const chain::util::decoded_types_data_storage& dtds, const std::string& decoded_state_objects_data, const bool throw_exception, const bool used_in_snapshot_plugin);

void verify_match_of_blockchain_configuration(fc::mutable_variant_object current_blockchain_config, const fc::variant& full_current_blockchain_config_as_variant, const std::string& full_stored_blockchain_config_json, const uint32_t hardfork, const bool used_in_snapshot_plugin);

} } } // hive::chain::util
