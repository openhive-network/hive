#pragma once
#include <hive/chain/hive_fwd.hpp>

#include <hive/chain/account_object.hpp>
#include <hive/chain/account_object_multiindex.hpp>

namespace hive { namespace plugins { namespace metadata {

// The metadata plugin now uses the chain's account_metadata_object
// to avoid type_id conflicts. The chain::account_metadata_object
// is defined in hive/chain/detail/state/account_object.hpp
using hive::chain::account_metadata_object;
using hive::chain::account_metadata_id_type;
using hive::chain::by_account;
using hive::chain::account_metadata_index;

// Note: account_metadata_index is now defined in chain library
// at hive/chain/detail/state/account_object_multiindex.hpp

}}}