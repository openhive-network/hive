#pragma once

#include <string>

namespace chainbase { class database; }

namespace hive { namespace chain {
  std::string get_irreversible_block_details(chainbase::database& db);
}}
