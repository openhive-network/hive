#pragma once

#include <hive/protocol/authority.hpp>

#include <optional>

namespace hive { namespace protocol {

/**
 * Source of authorities and witness signing keys for authority verification.
 * Each getter returns empty optional when given account (witness) is not known.
 */
class authority_getter_i
{
public:
  virtual ~authority_getter_i() = default;

  virtual std::optional< authority > get_active( const string& ) const = 0;
  virtual std::optional< authority > get_owner( const string& ) const = 0;
  virtual std::optional< authority > get_posting( const string& ) const = 0;
  virtual std::optional< public_key_type > get_witness_key( const string& ) const = 0;
};

/// selects one of authority_getter_i::get_active/get_owner/get_posting
typedef std::optional< authority > ( authority_getter_i::*authority_getter_method )( const string& ) const;

struct required_authorities_type
{
  flat_set<hive::protocol::account_name_type> required_active;
  flat_set<hive::protocol::account_name_type> required_owner;
  flat_set<hive::protocol::account_name_type> required_posting;
  flat_set<hive::protocol::account_name_type> required_witness;
  vector<hive::protocol::authority> other;
};

} } // hive::protocol
