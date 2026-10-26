#include <hive/chain/detail/state/account_object_multiindex.hpp>
#include <hive/chain/detail/state/transaction_object_multiindex.hpp>
#include <hive/chain/detail/state/witness_objects.hpp>
#include <hive/chain/database_authority_getter.hpp>

#include <hive/chain/index.hpp>
#include <chainbase/chainbase.inl>

#include <hive/chain/util/type_registrar_definition.hpp>

namespace hive { namespace chain {

void initialize_core_indexes_02( database& db )
{
  HIVE_ADD_CORE_INDEX(db, account_authority_index);
  HIVE_ADD_CORE_INDEX(db, transaction_index);
}

std::optional<protocol::authority> database_authority_getter::get_active( const std::string& account_name ) const
{
  const auto* auth = _db.find< account_authority_object, by_account >( account_name );
  if( auth == nullptr )
    return {};
  return auth->get_active().to_authority();
}

std::optional<protocol::authority> database_authority_getter::get_owner( const std::string& account_name ) const
{
  const auto* auth = _db.find< account_authority_object, by_account >( account_name );
  if( auth == nullptr )
    return {};
  return auth->get_owner().to_authority();
}

std::optional<protocol::authority> database_authority_getter::get_posting( const std::string& account_name ) const
{
  const auto* auth = _db.find< account_authority_object, by_account >( account_name );
  if( auth == nullptr )
    return {};
  return auth->get_posting().to_authority();
}

std::optional<protocol::public_key_type> database_authority_getter::get_witness_key( const std::string& witness_name ) const
{
  const auto* witness = _db.find_witness( witness_name );
  if( witness == nullptr )
    return {};
  return witness->signing_key;
}

} }

HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::account_authority_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::transaction_index)

// Explicit template instantiations for chainbase::database methods
template const chainbase::generic_index<hive::chain::account_authority_index>& chainbase::database::get_index<hive::chain::account_authority_index>() const;
template chainbase::generic_index<hive::chain::account_authority_index>& chainbase::database::get_mutable_index<hive::chain::account_authority_index>();

template const chainbase::generic_index<hive::chain::transaction_index>& chainbase::database::get_index<hive::chain::transaction_index>() const;
template chainbase::generic_index<hive::chain::transaction_index>& chainbase::database::get_mutable_index<hive::chain::transaction_index>();

