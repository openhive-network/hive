#pragma once

#include <hive/protocol/types.hpp>

#include <string>
#include <utility>

namespace hive { namespace protocol {

  std::pair<public_key_type, std::string> generate_private_key_from_password(const std::string& account, const std::string& role, const std::string& password);

  std::string normalize_brain_key(std::string s);

  struct brain_key_info
  {
    std::string      brain_priv_key;
    public_key_type  pub_key;
    std::string      wif_priv_key;
  };

  brain_key_info suggest_brain_key();

}} // namespace hive { namespace protocol
