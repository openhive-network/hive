#define MTLK_FROM_VARIANT_ON_CONSUME_JSON

#include <hive/chain/hive_fwd.hpp>

#include <appbase/application.hpp>

#include <hive/plugins/database_api/database_api.hpp>
#include <hive/plugins/database_api/database_api_plugin.hpp>

#include <hive/protocol/get_config.hpp>
#include <hive/protocol/exceptions.hpp>
#include <hive/protocol/transaction_util.hpp>
#include <hive/protocol/forward_impacted.hpp>

#include <hive/chain/util/smt_token.hpp>

#include <hive/utilities/git_revision.hpp>

#include <../../../apis/block_api/include/hive/plugins/block_api/block_api_objects.hpp>

namespace hive { namespace app {

std::shared_ptr<hive::chain::full_block_type> from_variant_to_full_block_ptr(const fc::variant& v )
{

  hive::plugins::block_api::api_signed_block_object sb;

  fc::from_variant( v, sb );

  auto siz = sb.transactions.size();
  siz = siz;

  return hive::chain::full_block_type::create_from_signed_block(sb);
}

}}