#pragma once

#include <vector>
#include <string>

#include <hive/protocol/operations.hpp>

namespace hive::app
{

enum class key_t : std::int32_t
{
  OWNER,
  ACTIVE,
  POSTING,
  MEMO,
  WITNESS_SIGNING,
};

struct collected_keyauth_t
{
  std::string account_name;
  key_t key_kind = key_t::OWNER;
  uint32_t weight_threshold = 0;
  bool keyauth_variant = true;
  fc::ecc::public_key_data key_auth;
  std::string account_auth;
  hive::protocol::weight_type w = 0;

};

typedef std::vector<collected_keyauth_t> collected_keyauth_collection_t;
collected_keyauth_collection_t operation_get_keyauths(const hive::protocol::operation& op);
collected_keyauth_collection_t operation_get_genesis_keyauths();
collected_keyauth_collection_t operation_get_hf09_keyauths();

} // namespace hive::app
