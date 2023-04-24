#define MTLK_FROM_VARIANT_ON_CONSUME_JSON_HACK

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


namespace consensus_state_provider
{
std::shared_ptr<hive::chain::full_block_type> from_variant_to_full_block_ptr(const fc::variant& v, int block_num_debug )
{

  hive::plugins::block_api::api_signed_block_object sb;

  fc::from_variant( v, sb );

  if(block_num_debug == 994240 || block_num_debug == 1021529)
  {
    auto& op = sb.transactions[0].operations[0].get<hive::protocol::witness_update_operation>();
    op.props.account_creation_fee.symbol.ser = 0x4d4545545301;
    //4d 45 45 54 53   03 

  }





  return hive::chain::full_block_type::create_from_signed_block(sb);
}

}