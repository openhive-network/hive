#pragma once
#include <hive/chain/hive_fwd.hpp>
#include <fc/time.hpp>

#include <hive/plugins/chain/abstract_block_producer.hpp>
#include <hive/plugins/chain/chain_plugin.hpp>

namespace hive { namespace plugins { namespace witness {

class block_producer : public chain::abstract_block_producer
{
public:
  block_producer(chain::database& db) : _db(db) {}

  /**
    * This function contains block generation logic.
    *
    * Calling this function invokes a lockless write and therefore
    * should be avoided. Instead, one should call chain_plugin::generate_block().
    */
  virtual void generate_block( chain::generate_block_flow_control* generate_block_ctrl ) override;

private:
  chain::database& _db;

  void _generate_block( chain::generate_block_flow_control* generate_block_ctrl, fc::time_point_sec when,
    const chain::account_name_type& witness_owner, const fc::ecc::private_key& block_signing_private_key );

  void adjust_hardfork_version_vote(const chain::witness_object& witness, chain::signed_block_header& pending_block);

  void apply_pending_transactions(const chain::account_name_type& witness_owner,
                                  fc::time_point_sec when,
                                  chain::signed_block_header& pending_block,
                                  std::vector<std::shared_ptr<hive::chain::full_transaction_type>>& full_transactions);

};

} } } // hive::plugins::witness
