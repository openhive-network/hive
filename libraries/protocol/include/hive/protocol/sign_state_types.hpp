
#pragma once

#include <hive/protocol/authority.hpp>

namespace hive { namespace protocol {

typedef std::function<authority(const string&)> authority_getter;
typedef std::function<public_key_type(const string&)> witness_public_key_getter;

struct required_authorities_type
{
  flat_set<hive::protocol::account_name_type> required_active;
  flat_set<hive::protocol::account_name_type> required_owner;
  flat_set<hive::protocol::account_name_type> required_posting;
  flat_set<hive::protocol::account_name_type> required_witness;
  vector<hive::protocol::authority> other;
};

} } // hive::protocol