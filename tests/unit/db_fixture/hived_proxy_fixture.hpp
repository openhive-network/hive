#pragma once

#include "hived_fixture.hpp"

#include <fc/log/logger_config.hpp>
#include <fc/optional.hpp>

namespace hive { namespace chain {

class hived_proxy_fixture
{
public:
  fc::ecc::private_key init_account_priv_key = fc::ecc::private_key::regenerate( fc::sha256::hash( fc::string( "init_key" ) ) );
  public_key_type init_account_pub_key = init_account_priv_key.get_public_key();

  void reset_fixture(bool remove_db_files = true)
  {
    fixture.reset(new hived_fixture(remove_db_files));
  }

  hive::chain::database* db() const
  {
    FC_ASSERT(fixture);
    return fixture->db;
  }

  // database_fixture methods

  fc::ecc::private_key generate_private_key( fc::string seed = "init_key" )
  {
    FC_ASSERT(fixture);
    return fixture->generate_private_key(seed);
  }

  hive::plugins::chain::chain_plugin& get_chain_plugin() const
  {
    FC_ASSERT(fixture);
    return fixture->get_chain_plugin();
  }

  void generate_block(uint32_t skip = 0,
                      const fc::ecc::private_key& key = database_fixture::generate_private_key("init_key"),
                      int miss_blocks = 0)
  {
    FC_ASSERT(fixture);
    return fixture->generate_block(skip, key, miss_blocks);
  }

  uint32_t generate_blocks( const std::string& debug_key, uint32_t count, uint32_t skip )
  {
    FC_ASSERT(fixture);
    return fixture->generate_blocks(debug_key, count, skip);
  }

  uint32_t generate_blocks_until( const std::string& debug_key, const fc::time_point_sec& head_block_time,
    bool generate_sparsely, uint32_t skip )
  {
    FC_ASSERT(fixture);
    return fixture->generate_blocks_until(debug_key, head_block_time, generate_sparsely, skip);
  }

  uint32_t get_last_irreversible_block_num()
  {
    FC_ASSERT(fixture);
    return fixture->get_last_irreversible_block_num();
  }

  void generate_blocks(uint32_t block_count)
  {
    FC_ASSERT(fixture);
    return fixture->generate_blocks(block_count);
  }

  void generate_blocks(fc::time_point_sec timestamp, bool miss_intermediate_blocks = true)
  {
    FC_ASSERT(fixture);
    return fixture->generate_blocks(timestamp, miss_intermediate_blocks);
  }

  void generate_seconds_blocks( uint32_t seconds, bool skip_interm_blocks = true )
  {
    FC_ASSERT(fixture);
    return fixture->generate_seconds_blocks(seconds, skip_interm_blocks);
  }
  void generate_days_blocks( uint32_t days, bool skip_interm_blocks = true )
  {
    FC_ASSERT(fixture);
    return fixture->generate_days_blocks(days, skip_interm_blocks);
  }
  void generate_until_block( uint32_t block_num )
  {
    FC_ASSERT(fixture);
    return fixture->generate_until_block(block_num);
  }
  void generate_until_irreversible_block( uint32_t block_num )
  {
    FC_ASSERT(fixture);
    return fixture->generate_until_irreversible_block(block_num);
  }

  fc::string get_current_time_iso_string() const
  {
    FC_ASSERT(fixture);
    return fixture->get_current_time_iso_string();
  }

  const account_object& account_create(
    const fc::string& name,
    const fc::string& creator,
    const private_key_type& creator_key,
    const share_type& fee,
    const public_key_type& key,
    const public_key_type& post_key,
    const fc::string& json_metadata
  )
  {
    FC_ASSERT(fixture);
    return fixture->account_create(name, creator, creator_key, fee, key, post_key, json_metadata);
  }

  const account_object& account_create(
    const fc::string& name,
    const public_key_type& key,
    const public_key_type& post_key
  )
  {
    FC_ASSERT(fixture);
    return fixture->account_create(name, key, post_key);
  }

  const account_object& account_create_default_fee(
    const fc::string& name,
    const public_key_type& key,
    const public_key_type& post_key
  )
  {
    FC_ASSERT(fixture);
    return fixture->account_create_default_fee(name, key, post_key);
  }

  const account_object& account_create(
    const fc::string& name,
    const public_key_type& key
  )
  {
    FC_ASSERT(fixture);
    return fixture->account_create(name, key);
  }

  const witness_object& witness_create(
    const fc::string& owner,
    const private_key_type& owner_key,
    const fc::string& url,
    const public_key_type& signing_key,
    const share_type& fee
  )
  {
    FC_ASSERT(fixture);
    return fixture->witness_create(owner, owner_key, url, signing_key, fee);
  }

  void account_update( const fc::string& account, const fc::ecc::public_key& memo_key, const fc::string& metadata,
                       fc::optional<authority> owner, fc::optional<authority> active, fc::optional<authority> posting,
                       const fc::ecc::private_key& key )
  {
    FC_ASSERT(fixture);
    return fixture->account_update(account, memo_key, metadata, owner, active, posting, key);
  }
  void account_update2( const fc::string& account, fc::optional<authority> owner, fc::optional<authority> active, fc::optional<authority> posting,
                        fc::optional<public_key_type> memo_key, const fc::string& metadata, const fc::string& posting_metadata,
                        const fc::ecc::private_key& key )
  {
    FC_ASSERT(fixture);
    return fixture->account_update2(account, owner, active, posting, memo_key, metadata, posting_metadata, key);
  }

  void push_transaction( const operation& op, const fc::ecc::private_key& key )
  {
    FC_ASSERT(fixture);
    return fixture->push_transaction(op, key);
  }
  full_transaction_ptr push_transaction( const signed_transaction& tx, const fc::ecc::private_key& key = fc::ecc::private_key(),
    uint32_t skip_flags = 0, hive::protocol::pack_type pack_type = hive::protocol::serialization_mode_controller::get_current_pack(),
    fc::ecc::canonical_signature_type _sig_type = fc::ecc::fc_canonical )
  {
    FC_ASSERT(fixture);
    return fixture->push_transaction(tx, key, skip_flags, pack_type, _sig_type);
  }
  full_transaction_ptr push_transaction( const signed_transaction& tx, const std::vector<fc::ecc::private_key>& keys,
    uint32_t skip_flags = 0, hive::protocol::pack_type pack_type = hive::protocol::serialization_mode_controller::get_current_pack(),
    fc::ecc::canonical_signature_type _sig_type = fc::ecc::fc_canonical )
  {
    FC_ASSERT(fixture);
    return fixture->push_transaction(tx, keys, skip_flags, pack_type, _sig_type);
  }

  void fund( const fc::string& account_name, const asset& amount )
  {
    FC_ASSERT(fixture);
    return fixture->fund(account_name, amount);
  }
  void issue_funds( const fc::string& account_name, const asset& amount, bool update_print_rate = true )
  {
    FC_ASSERT(fixture);
    return fixture->issue_funds(account_name, amount, update_print_rate);
  }
  void transfer( const fc::string& from, const fc::string& to, const asset& amount, const std::string& memo, const fc::ecc::private_key& key )
  {
    FC_ASSERT(fixture);
    return fixture->transfer(from, to, amount, memo, key);
  }
  void recurrent_transfer( const fc::string& from, const fc::string& to, const asset& amount, const fc::string& memo, uint16_t recurrence,
                           uint16_t executions, const fc::ecc::private_key& key )
  {
    FC_ASSERT(fixture);
    return fixture->recurrent_transfer(from, to, amount, memo, recurrence, executions, key);
  }
  void convert_hbd_to_hive( const std::string& owner, uint32_t requestid, const asset& amount, const fc::ecc::private_key& key )
  {
    FC_ASSERT(fixture);
    return fixture->convert_hbd_to_hive(owner, requestid, amount, key);
  }
  void collateralized_convert_hive_to_hbd( const std::string& owner, uint32_t requestid, const asset& amount, const fc::ecc::private_key& key )
  {
    FC_ASSERT(fixture);
    return fixture->collateralized_convert_hive_to_hbd(owner, requestid, amount, key);
  }
  void vest( const fc::string& to, const asset& amount )
  {
    FC_ASSERT(fixture);
    return fixture->vest(to, amount);
  }
  void vest( const fc::string& from, const fc::string& to, const asset& amount, const fc::ecc::private_key& key )
  {
    FC_ASSERT(fixture);
    return fixture->vest(from, to, amount, key);
  }
  void delegate_vest( const fc::string& delegator, const fc::string& delegatee, const asset& amount, const fc::ecc::private_key& key )
  {
    FC_ASSERT(fixture);
    return fixture->delegate_vest(delegator, delegatee, amount, key);
  }
  void set_withdraw_vesting_route(const fc::string& from, const fc::string& to, uint16_t percent, bool auto_vest, const fc::ecc::private_key& key)
  {
    FC_ASSERT(fixture);
    return fixture->set_withdraw_vesting_route(from, to, percent, auto_vest, key);
  }
  void withdraw_vesting( const fc::string& account, const asset& amount, const fc::ecc::private_key& key )
  {
    FC_ASSERT(fixture);
    return fixture->withdraw_vesting(account, amount, key);
  }
  void proxy( const fc::string& _account, const fc::string& _proxy, const fc::ecc::private_key& _key )
  {
    FC_ASSERT(fixture);
    return fixture->proxy(_account, _proxy, _key);
  }
  void set_price_feed( const price& new_price, bool stop_at_update_block = false )
  {
    FC_ASSERT(fixture);
    return fixture->set_price_feed(new_price, stop_at_update_block);
  }
  void set_witness_props( const boost::container::flat_map< fc::string, std::vector< char > >& new_props )
  {
    FC_ASSERT(fixture);
    return fixture->set_witness_props(new_props);
  }
  void witness_feed_publish( const fc::string& publisher, const price& exchange_rate, const private_key_type& key )
  {
    FC_ASSERT(fixture);
    return fixture->witness_feed_publish(publisher, exchange_rate, key);
  }
  share_type get_votes( const fc::string& witness_name )
  {
    FC_ASSERT(fixture);
    return fixture->get_votes(witness_name);
  }
  void witness_vote( account_name_type voter, account_name_type witness, const fc::ecc::private_key& key, bool approve = true )
  {
    FC_ASSERT(fixture);
    return fixture->witness_vote(voter, witness, key, approve);
  }
  void limit_order_create( const fc::string& owner, const asset& amount_to_sell, const asset& min_to_receive, bool fill_or_kill,
                           const fc::microseconds& expiration_shift, uint32_t orderid, const fc::ecc::private_key& key)
  {
    FC_ASSERT(fixture);
    return fixture->limit_order_create(owner, amount_to_sell, min_to_receive, fill_or_kill, expiration_shift, orderid, key);
  }
  void limit_order_cancel( const fc::string& owner, uint32_t orderid, const fc::ecc::private_key& key )
  {
    FC_ASSERT(fixture);
    return fixture->limit_order_cancel(owner, orderid, key);
  }
  void limit_order2_create( const fc::string& owner, const asset& amount_to_sell, const price& exchange_rate, bool fill_or_kill,
                            const fc::microseconds& expiration_shift, uint32_t orderid, const fc::ecc::private_key& key )
  {
    FC_ASSERT(fixture);
    return fixture->limit_order2_create(owner, amount_to_sell, exchange_rate, fill_or_kill, expiration_shift, orderid, key);
  }
  void escrow_transfer( const fc::string& from, const fc::string& to, const fc::string& agent, const asset& hive_amount, 
                        const asset& hbd_amount, const asset& fee, const std::string& json_meta, const fc::microseconds& ratification_shift,
                        const fc::microseconds& expiration_shift, uint32_t escrow_id, const fc::ecc::private_key& key )
  {
    FC_ASSERT(fixture);
    return fixture->escrow_transfer(from, to, agent, hive_amount, hbd_amount, fee, json_meta, ratification_shift, expiration_shift, escrow_id, key);
  }
  void escrow_approve( const fc::string& from, const fc::string& to, const fc::string& agent, const fc::string& who, bool approve, uint32_t escrow_id, const fc::ecc::private_key& key )
  {
    FC_ASSERT(fixture);
    return fixture->escrow_approve(from, to, agent, who, approve, escrow_id, key);
  }
  void escrow_release( const fc::string& from, const fc::string& to, const fc::string& agent, const fc::string& who, const fc::string& receiver,
                       const asset& hive_amount, const asset& hbd_amount, uint32_t escrow_id, const fc::ecc::private_key& key )
  {
    FC_ASSERT(fixture);
    return fixture->escrow_release(from, to, agent, who, receiver, hive_amount, hbd_amount, escrow_id, key);
  }
  void escrow_dispute( const fc::string& from, const fc::string& to, const fc::string& agent, const fc::string& who, uint32_t escrow_id, const fc::ecc::private_key& key )
  {
    FC_ASSERT(fixture);
    return fixture->escrow_dispute(from, to, agent, who, escrow_id, key);
  }
  void transfer_to_savings( const fc::string& from, const fc::string& to, const asset& amount, const fc::string& memo, const fc::ecc::private_key& key )
  {
    FC_ASSERT(fixture);
    return fixture->transfer_to_savings(from, to, amount, memo, key);
  }
  void transfer_from_savings( const fc::string& from, const fc::string& to, const asset& amount, const fc::string& memo, uint32_t request_id,
                              const fc::ecc::private_key& key )
  {
    FC_ASSERT(fixture);
    return fixture->transfer_from_savings(from, to, amount, memo, request_id, key);
  }
  void cancel_transfer_from_savings( const fc::string& from, uint32_t request_id, const fc::ecc::private_key& key )
  {
    FC_ASSERT(fixture);
    return fixture->cancel_transfer_from_savings(from, request_id, key);
  }

  void push_custom_operation( const flat_set< account_name_type >& required_auths, uint16_t id,
                              const std::vector< char >& data, const fc::ecc::private_key& key )
  {
    FC_ASSERT(fixture);
    return fixture->push_custom_operation(required_auths, id, data, key);
  }
  void push_custom_json_operation( const flat_set< account_name_type >& required_auths, 
                                   const flat_set< account_name_type >& required_posting_auths,
                                   const custom_id_type& id, const std::string& json, const fc::ecc::private_key& key )
  {
    FC_ASSERT(fixture);
    return fixture->push_custom_json_operation(required_auths, required_posting_auths, id, json, key);
  }

  void decline_voting_rights( const fc::string& account, const bool decline, const fc::ecc::private_key& key )
  {
    FC_ASSERT(fixture);
    return fixture->decline_voting_rights(account, decline, key);
  }

  int64_t create_proposal( const std::string& creator, const std::string& receiver, const std::string& subject, const std::string& permlink,
                           fc::time_point_sec start_date, fc::time_point_sec end_date, asset daily_pay, const fc::ecc::private_key& key )
  {
    FC_ASSERT(fixture);
    return fixture->create_proposal(creator, receiver, subject, permlink, start_date, end_date, daily_pay, key);
  }
  int64_t create_proposal(   std::string creator, std::string receiver,
                    fc::time_point_sec start_date, fc::time_point_sec end_date,
                    asset daily_pay, const fc::ecc::private_key& key, bool with_block_generation = true )
  {
    FC_ASSERT(fixture);
    return fixture->create_proposal(creator, receiver, start_date, end_date, daily_pay, key, with_block_generation);
  }
  void vote_proposal( std::string voter, const std::vector< int64_t >& id_proposals, bool approve, const fc::ecc::private_key& key )
  {
    FC_ASSERT(fixture);
    return fixture->vote_proposal(voter, id_proposals, approve, key);
  }
  void remove_proposal(account_name_type _deleter, flat_set<int64_t> _proposal_id, const fc::ecc::private_key& _key)
  {
    FC_ASSERT(fixture);
    return fixture->remove_proposal(_deleter, _proposal_id, _key);
  }
  void update_proposal(uint64_t proposal_id, std::string creator, asset daily_pay, std::string subject, std::string permlink, const fc::ecc::private_key& key, fc::time_point_sec* end_date = nullptr )
  {
    FC_ASSERT(fixture);
    return fixture->update_proposal(proposal_id, creator, daily_pay, subject, permlink, key, end_date);
  }

  bool exist_proposal( int64_t id )
  {
    FC_ASSERT(fixture);
    return fixture->exist_proposal(id);
  }
  const proposal_object* find_proposal( int64_t id )
  {
    FC_ASSERT(fixture);
    return fixture->find_proposal(id);
  }
  const proposal_vote_object* find_vote_for_proposal(const std::string& _user, int64_t _proposal_id)
  {
    FC_ASSERT(fixture);
    return fixture->find_vote_for_proposal(_user, _proposal_id);
  }
  uint64_t get_nr_blocks_until_proposal_maintenance_block()
  {
    FC_ASSERT(fixture);
    return fixture->get_nr_blocks_until_proposal_maintenance_block();
  }
  uint64_t get_nr_blocks_until_daily_proposal_maintenance_block()
  {
    FC_ASSERT(fixture);
    return fixture->get_nr_blocks_until_daily_proposal_maintenance_block();
  }

  account_id_type get_account_id( const fc::string& account_name )const
  {
    FC_ASSERT(fixture);
    return fixture->get_account_id(account_name);
  }
  asset get_balance( const fc::string& account_name )const
  {
    FC_ASSERT(fixture);
    return fixture->get_balance(account_name);
  }
  asset get_hbd_balance( const fc::string& account_name )const
  {
    FC_ASSERT(fixture);
    return fixture->get_hbd_balance(account_name);
  }
  asset get_savings( const fc::string& account_name )const
  {
    FC_ASSERT(fixture);
    return fixture->get_savings(account_name);
  }
  asset get_hbd_savings( const fc::string& account_name )const
  {
    FC_ASSERT(fixture);
    return fixture->get_hbd_savings(account_name);
  }
  asset get_rewards( const fc::string& account_name )const
  {
    FC_ASSERT(fixture);
    return fixture->get_rewards(account_name);
  }
  asset get_hbd_rewards( const fc::string& account_name )const
  {
    FC_ASSERT(fixture);
    return fixture->get_hbd_rewards(account_name);
  }
  asset get_vesting( const fc::string& account_name )const
  {
    FC_ASSERT(fixture);
    return fixture->get_vesting(account_name);
  }
  asset get_vest_rewards( const fc::string& account_name )const
  {
    FC_ASSERT(fixture);
    return fixture->get_vest_rewards(account_name);
  }
  asset get_vest_rewards_as_hive( const fc::string& account_name )const
  {
    FC_ASSERT(fixture);
    return fixture->get_vest_rewards_as_hive(account_name);
  }

  bool compare_delayed_vote_count( const account_name_type& name, const std::vector<uint64_t>& data_to_compare )
  {
    FC_ASSERT(fixture);
    return fixture->compare_delayed_vote_count(name, data_to_compare);
  }

  std::vector< operation > get_last_operations( uint32_t ops )
  {
    FC_ASSERT(fixture);
    return fixture->get_last_operations(ops);
  }

  void validate_database()
  {
    FC_ASSERT(fixture);
    return fixture->validate_database();
  }

  // hived_fixture methods

  template< typename... Plugin >
  void postponed_init( const hived_fixture::config_arg_override_t& config_arg_overrides = hived_fixture::config_arg_override_t(), Plugin**... plugin_ptr )
  {
    FC_ASSERT(fixture);
    return fixture->postponed_init(config_arg_overrides, plugin_ptr...);
  }

private:
  std::unique_ptr<hived_fixture> fixture;
};

} }
