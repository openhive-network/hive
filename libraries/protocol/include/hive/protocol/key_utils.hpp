#pragma once

#include <hive/protocol/types.hpp>

#include <string>
#include <utility>

namespace hive { namespace words { class word_list; } }

namespace hive { namespace protocol {

  std::pair<public_key_type, std::string> generate_private_key_from_password(const std::string& account, const std::string& role, const std::string& password);

  std::string normalize_brain_key(std::string s);

  struct brain_key_info
  {
    std::string      brain_priv_key;
    public_key_type  pub_key;
    std::string      wif_priv_key;
  };

  /// Brain key of words from the word list built into the library
  brain_key_info suggest_brain_key();
  /// Brain key of words from the given word list
  brain_key_info suggest_brain_key(const hive::words::word_list& words);

}} // namespace hive { namespace protocol
