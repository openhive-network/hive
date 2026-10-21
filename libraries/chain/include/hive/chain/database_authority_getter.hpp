#pragma once

#include <hive/protocol/sign_state_types.hpp>

namespace hive { namespace chain {

class database;

/**
 * Provides authorities of accounts and signing keys of witnesses from current state.
 */
class database_authority_getter : public protocol::authority_getter_i
{
public:
  explicit database_authority_getter( const database& db ) : _db( db ) {}

  virtual std::optional<protocol::authority> get_active( const std::string& account_name ) const override;
  virtual std::optional<protocol::authority> get_owner( const std::string& account_name ) const override;
  virtual std::optional<protocol::authority> get_posting( const std::string& account_name ) const override;
  virtual std::optional<protocol::public_key_type> get_witness_key( const std::string& witness_name ) const override;

protected:
  const database& _db;
};

} } // hive::chain
