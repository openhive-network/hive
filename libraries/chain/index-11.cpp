#include <hive/chain/comment_object_multiindex.hpp>
#include <hive/chain/dhf_objects_multiindex.hpp>
#include <hive/chain/detail/state/recurrent_transfer_object_multiindex.hpp>

#include <hive/chain/index.hpp>
#include <chainbase/chainbase.inl>

#include <hive/chain/util/type_registrar_definition.hpp>

namespace hive { namespace chain {

void initialize_core_indexes_11( database& db )
{
  HIVE_ADD_CORE_INDEX(db, proposal_vote_index);
  HIVE_ADD_CORE_INDEX(db, comment_cashout_index);
  HIVE_ADD_CORE_INDEX(db, comment_cashout_ex_index);
  HIVE_ADD_CORE_INDEX(db, recurrent_transfer_index);
}

} }

HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::proposal_vote_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::comment_cashout_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::comment_cashout_ex_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::recurrent_transfer_index)

// Explicit template instantiations for chainbase::database methods
template const chainbase::generic_index<hive::chain::proposal_vote_index>& chainbase::database::get_index<hive::chain::proposal_vote_index>() const;
template chainbase::generic_index<hive::chain::proposal_vote_index>& chainbase::database::get_mutable_index<hive::chain::proposal_vote_index>();

template const chainbase::generic_index<hive::chain::comment_cashout_index>& chainbase::database::get_index<hive::chain::comment_cashout_index>() const;
template chainbase::generic_index<hive::chain::comment_cashout_index>& chainbase::database::get_mutable_index<hive::chain::comment_cashout_index>();

template const chainbase::generic_index<hive::chain::comment_cashout_ex_index>& chainbase::database::get_index<hive::chain::comment_cashout_ex_index>() const;
template chainbase::generic_index<hive::chain::comment_cashout_ex_index>& chainbase::database::get_mutable_index<hive::chain::comment_cashout_ex_index>();

template const chainbase::generic_index<hive::chain::recurrent_transfer_index>& chainbase::database::get_index<hive::chain::recurrent_transfer_index>() const;
template chainbase::generic_index<hive::chain::recurrent_transfer_index>& chainbase::database::get_mutable_index<hive::chain::recurrent_transfer_index>();