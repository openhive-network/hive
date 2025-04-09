#include <hive/chain/util/state_checker_tools.hpp>

#include <hive/chain/database_exceptions.hpp>
#include <hive/protocol/config.hpp>

#include <fc/exception/exception.hpp>

#include <fstream>

namespace hive { namespace chain { namespace util {

void verify_match_of_state_definitions(const chain::util::decoded_types_data_storage& dtds, const std::string& decoded_state_objects_data)
{
  auto result = dtds.check_if_decoded_types_data_json_matches_with_current_decoded_data(decoded_state_objects_data);

  if (result.first != state_definitions_verification_result::state_definitions_matches)
  {
    std::fstream loaded_decoded_types_details, current_decoded_types_details;
    constexpr char current_data_filename[] = "current_decoded_types_details.log";
    const std::string loaded_data_filename = "loaded_from_shm_decoded_types_details.log";

    loaded_decoded_types_details.open(loaded_data_filename, std::ios::out | std::ios::trunc);
    if (loaded_decoded_types_details.good())
      loaded_decoded_types_details << dtds.generate_decoded_types_data_pretty_string(decoded_state_objects_data);
    loaded_decoded_types_details.flush();
    loaded_decoded_types_details.close();

    current_decoded_types_details.open(current_data_filename, std::ios::out | std::ios::trunc);
    if (current_decoded_types_details.good())
      current_decoded_types_details << dtds.generate_decoded_types_data_pretty_string();
    current_decoded_types_details.flush();
    current_decoded_types_details.close();

    if (result.first == state_definitions_verification_result::mismatch_throw_error)
      FC_THROW_EXCEPTION(hive::chain::shm_state_definitions_mismatch_exception,
                        "Details:\n ${details}"
                        "\nFull data about decoded state objects are in files: ${current_data_filename}, ${loaded_data_filename}",
                        ("details", result.second)(current_data_filename)(loaded_data_filename));
    else
      wlog("Mismatch between current and loaded from shm state definitions. Details:\n ${details}"
            "\nFull data about decoded state objects are in files: ${current_data_filename}, ${loaded_data_filename}",
            ("details", result.second)(current_data_filename)(loaded_data_filename));
  }
}

void verify_match_of_blockchain_configuration(fc::mutable_variant_object current_blockchain_config, const fc::variant &full_current_blockchain_config_as_variant, const std::string &full_stored_blockchain_config_json, const uint32_t hardfork)
{
  constexpr char HIVE_TREASURY_ACCOUNT_KEY[] = "HIVE_TREASURY_ACCOUNT";
  constexpr char HIVE_CHAIN_ID_KEY[] = "HIVE_CHAIN_ID";
  constexpr char HIVE_BLOCKCHAIN_VERSION_KEY[] = "HIVE_BLOCKCHAIN_VERSION";

  fc::mutable_variant_object stored_blockchain_config = fc::json::from_string(full_stored_blockchain_config_json, fc::json::format_validation_mode::full).get_object();
  const std::string current_hive_chain_id = current_blockchain_config[HIVE_CHAIN_ID_KEY].as_string();
  const std::string stored_hive_chain_id = stored_blockchain_config[HIVE_CHAIN_ID_KEY].as_string();

#if defined(USE_ALTERNATE_CHAIN_ID) || defined(IS_TEST_NET)
  // mirrornet & testnet  
  if (current_hive_chain_id != stored_hive_chain_id)
    FC_THROW_EXCEPTION(blockchain_config_mismatch_exception, "Chain id stored in database: ${stored_hive_chain_id} mismatch chain-id from configuration: ${current_hive_chain_id}", (stored_hive_chain_id)(current_hive_chain_id));
#else
  // mainnet
  if ((hardfork < HIVE_HARDFORK_1_24 && (current_hive_chain_id != std::string(OLD_CHAIN_ID) || current_hive_chain_id != stored_hive_chain_id)) ||
      (hardfork >= HIVE_HARDFORK_1_24 && (current_hive_chain_id != std::string(HIVE_CHAIN_ID) || current_hive_chain_id != stored_hive_chain_id)))
    FC_THROW_EXCEPTION(blockchain_config_mismatch_exception, "chain id mismatch. Current config: ${current_hive_chain_id}, stored in db: ${stored_hive_chain_id}, hf: ${hardfork}", (current_hive_chain_id)(stored_hive_chain_id)(hardfork));
#endif

  stored_blockchain_config.erase(HIVE_TREASURY_ACCOUNT_KEY);
  current_blockchain_config.erase(HIVE_TREASURY_ACCOUNT_KEY);
  stored_blockchain_config.erase(HIVE_CHAIN_ID_KEY);
  current_blockchain_config.erase(HIVE_CHAIN_ID_KEY);
  stored_blockchain_config.erase(HIVE_BLOCKCHAIN_VERSION_KEY);
  current_blockchain_config.erase(HIVE_BLOCKCHAIN_VERSION_KEY);

  fc::variant modified_current_blockchain_config;
  fc::to_variant(current_blockchain_config, modified_current_blockchain_config);
  fc::variant modified_stored_blockchain_config;
  fc::to_variant(stored_blockchain_config, modified_stored_blockchain_config);

  if (fc::json::to_string(modified_current_blockchain_config) != fc::json::to_string(modified_stored_blockchain_config))
  {
    std::fstream loaded_blockchain_config_file, current_blockchain_config_file;
    constexpr char current_config_filename[] = "current_blockchain_config.log";
    const std::string loaded_config_filename = "loaded_from_shm_blockchain_config.log";

    loaded_blockchain_config_file.open(loaded_config_filename, std::ios::out | std::ios::trunc);
    if (loaded_blockchain_config_file.good())
      loaded_blockchain_config_file << fc::json::to_pretty_string(fc::json::from_string(full_stored_blockchain_config_json, fc::json::format_validation_mode::full));
    loaded_blockchain_config_file.flush();
    loaded_blockchain_config_file.close();

    current_blockchain_config_file.open(current_config_filename, std::ios::out | std::ios::trunc);
    if (current_blockchain_config_file.good())
      current_blockchain_config_file << fc::json::to_pretty_string(full_current_blockchain_config_as_variant);
    current_blockchain_config_file.flush();
    current_blockchain_config_file.close();

    FC_THROW_EXCEPTION(blockchain_config_mismatch_exception,
                       "Mismatch between blockchain configuration loaded from shared memory file and the current one"
                       "\nFull data about blockchain configuration are in files: ${current_config_filename}, ${loaded_config_filename}",
                       (current_config_filename)(loaded_config_filename));
  }
}
    } } } // hive::chain::util