#include <hive/protocol/forward_keyauths.hpp>

#include <hive/protocol/authority.hpp>
#include <hive/protocol/forward_impacted.hpp>


namespace hive::app
{

using namespace fc;
using namespace hive::protocol;



struct keyauth_collector
{
  collected_keyauth_collection_t collected_keyauths;

  typedef void result_type;

private:


  uint32_t get_weight_threshold(const authority& _authority) const
  {
    return _authority.weight_threshold;
  }

  uint32_t get_weight_threshold(const optional<authority>& _authority) const
  {
    if(_authority)
      return _authority->weight_threshold;
    return {};
  }

  hive::protocol::authority::key_authority_map get_key_auths(const authority& _authority) const
  {
    return _authority.key_auths;
  }

  hive::protocol::authority::key_authority_map get_key_auths(const optional<authority>& _authority) const
  {
    if(_authority)
      return _authority->key_auths;
    return {};
  }

  void collect_key_auths(const authority& _authority, std::string _account_name, key_t _key_kind, uint32_t _weight_threshold)
  {
    for(const auto& [key, weight]: _authority.key_auths)
    {
      collected_keyauths.emplace_back(collected_keyauth_t{_account_name, _key_kind, _weight_threshold, true, key, {}, weight});
    }
  }

  void collect_key_auths(const optional<authority>& _authority, std::string _account_name, key_t _key_kind, uint32_t _weight_threshold)
  {
    if(_authority)
      collect_key_auths(*_authority, _account_name, _key_kind, _weight_threshold);
  }

  void collect_account_auths(const authority& _authority, std::string _account_name, key_t _key_kind, uint32_t _weight_threshold)
  {
    // If empty, add an entry to 'collected_keyauths' for overriding the former entry in PostgreSQL table.
    if(_authority.account_auths.empty())
      collected_keyauths.emplace_back(collected_keyauth_t{_account_name, _key_kind, _weight_threshold, false, {}, "", 0});
    else
      for(const auto& [account, weight]: _authority.account_auths)
        collected_keyauths.emplace_back(collected_keyauth_t{_account_name, _key_kind, _weight_threshold, false, {}, account, weight});
  }

  void collect_account_auths(const optional<authority>& _authority, std::string _account_name, key_t _key_kind, uint32_t _weight_threshold)
  {
    if(_authority)
      collect_account_auths(*_authority, _account_name, _key_kind, _weight_threshold);
  }

  void collect_memo_key(public_key_type memo_key, collected_keyauth_t& collected_item) 
  {
    if( memo_key != public_key_type() )
    {
      collected_item.key_auth = memo_key;
      collected_keyauths.emplace_back(collected_item);
    }
  }


  void collect_memo_key(optional< public_key_type > memo_key, collected_keyauth_t& collected_item) 
  {
    if(memo_key)
      collect_memo_key(*memo_key, collected_item);
  }


  template<typename T, typename AT>
  void collect_one(
    const T& _op,
    const AT& _authority,
    key_t _key_kind,
    std::string _account_name
    )
  {
    auto weight_threshold = get_weight_threshold(_authority);

    // For key authorizations
    collect_key_auths(_authority, _account_name, _key_kind, weight_threshold);

    // For account authorizations
    collect_account_auths(_authority, _account_name, _key_kind, weight_threshold);
  }

  template<typename T>
  void collect_memo(const T& op, std::string _account_name)
  {
    collected_keyauth_t collected_item;
    collected_item.account_name   = _account_name;
    collected_item.key_kind = hive::app::key_t::MEMO;

    collect_memo_key(op.memo_key, collected_item);
  }



  template<typename T>
  void collect_witness_signing(const T& op, const account_name_type& worker_account_name)
  {
    vector< authority > authorities;
    op.get_required_authorities(authorities);
    for(const auto& authority : authorities)
    {
      collect_one(op, authority, hive::app::key_t::WITNESS_SIGNING,  worker_account_name);
    }
  }


  void collect_pow(const account_name_type& worker_account_name, const public_key_type& worker_key)
  {
    collected_keyauths.emplace_back(collected_keyauth_t{worker_account_name, hive::app::key_t::OWNER, 0, true, worker_key, {}, 0});
    collected_keyauths.emplace_back(collected_keyauth_t{worker_account_name, hive::app::key_t::ACTIVE, 0, true, worker_key, {}, 0});
    collected_keyauths.emplace_back(collected_keyauth_t{worker_account_name, hive::app::key_t::POSTING, 0, true, worker_key, {}, 0});
    collected_keyauths.emplace_back(collected_keyauth_t{worker_account_name, hive::app::key_t::MEMO, 0, true, worker_key, {}, 0});
  }

  public:


  // ops
  result_type operator()( const pow_operation& op )
  {
    collect_pow(op.worker_account, op.work.worker);
  }


  struct keyauth_pow2_visitor 
  {
      using result_type = account_name_type;

      template <typename T>
      account_name_type operator()(const T& work) const
      {
        return work.input.worker_account;
      }

  };

  result_type operator()( const pow2_operation& op )
  {
    account_name_type worker_account = op.work.visit(keyauth_pow2_visitor());

    if(op.new_owner_key)
    {
      collect_pow(worker_account, *op.new_owner_key);
    }

    collect_witness_signing(op, worker_account);
  }

  result_type operator()( const account_create_operation& op )
  {
    collect_one(op, op.owner,   hive::app::key_t::OWNER,   op.new_account_name);
    collect_one(op, op.active,  hive::app::key_t::ACTIVE,  op.new_account_name);
    collect_one(op, op.posting, hive::app::key_t::POSTING, op.new_account_name);
    collect_memo(op, op.new_account_name);
  }

  result_type operator()( const account_create_with_delegation_operation& op )
  {
    collect_one(op, op.owner,   hive::app::key_t::OWNER,   op.new_account_name);
    collect_one(op, op.active,  hive::app::key_t::ACTIVE,  op.new_account_name);
    collect_one(op, op.posting, hive::app::key_t::POSTING, op.new_account_name);
    collect_memo(op, op.new_account_name);
  }

  result_type operator()( const account_update_operation& op )
  {
    collect_one(op, op.owner,  hive::app::key_t::OWNER,  op.account);
    collect_one(op, op.active, hive::app::key_t::ACTIVE, op.account);
    collect_one(op, op.posting,hive::app::key_t::POSTING, op.account);
    collect_memo(op, op.account);
  }

  result_type operator()( const account_update2_operation& op )
  {
    collect_one(op, op.owner,  hive::app::key_t::OWNER,  op.account);
    collect_one(op, op.active, hive::app::key_t::ACTIVE, op.account);
    collect_one(op, op.posting,hive::app::key_t::POSTING, op.account);
    collect_memo(op, op.account);
  }

  result_type operator()( const create_claimed_account_operation& op )
  {
    collect_one(op, op.owner,  hive::app::key_t::OWNER,  op.new_account_name);
    collect_one(op, op.active, hive::app::key_t::ACTIVE, op.new_account_name);
    collect_one(op, op.posting,hive::app::key_t::POSTING, op.new_account_name);
    collect_memo(op, op.new_account_name);
  }

  result_type operator()( const recover_account_operation& op)
  {
    collect_one(op, op.new_owner_authority, hive::app::key_t::OWNER, op.account_to_recover);
  }

  result_type operator()( const reset_account_operation& op)
  {
    collect_one(op, op.new_owner_authority, hive::app::key_t::OWNER, op.account_to_reset);
  }

  result_type operator()( const witness_set_properties_operation& op )
  {
    collect_witness_signing(op, op.owner);
  }

  result_type operator()( const witness_update_operation& op )
  {
    collected_keyauths.emplace_back(collected_keyauth_t{op.owner, hive::app::key_t::WITNESS_SIGNING, 0, true, op.block_signing_key, {}, 0});
  }

  template <typename T>
  void operator()(const T& op)
  {
    exclude_from_used_operations<T>(used_operations);
  }

  hive::app::stringset used_operations;
};

collected_keyauth_collection_t operation_get_keyauths(const hive::protocol::operation& op)
{
  keyauth_collector collector;

  op.visit(collector);

  return std::move(collector.collected_keyauths);
}



// Helper function to add key authorizations for different key types
void add_key_authorizations(keyauth_collector& collector, 
                            collected_keyauth_t collected_item, 
                            const std::vector<key_t>& key_types) 
{
    for (auto& key_type : key_types) {
        collected_item.key_kind = key_type;
        collector.collected_keyauths.emplace_back(collected_item);
    }
}

collected_keyauth_collection_t operation_get_genesis_keyauths() 
{
    keyauth_collector collector;

    // Common key types for genesis accounts
    std::vector<key_t> genesis_key_types = {key_t::OWNER, key_t::ACTIVE, key_t::POSTING, key_t::MEMO};

    // 'steem' account
    auto STEEM_PUBLIC_KEY = public_key_type(HIVE_ADDRESS_PREFIX HIVE_STEEM_PUBLIC_KEY_BODY);
    collected_keyauth_t steem_item {"steem", key_t::OWNER, 1, true, STEEM_PUBLIC_KEY, {}, 1};
    add_key_authorizations(collector, steem_item, genesis_key_types);

    // 'initminer' account
    auto INITMINER_KEY = public_key_type(HIVE_INIT_PUBLIC_KEY_STR);
    collected_keyauth_t initminer_item {"initminer", key_t::OWNER, 0, true, INITMINER_KEY, {}, 0};
    add_key_authorizations(collector, initminer_item, genesis_key_types);

    // Other genesis accounts - only MEMO
    std::vector<std::string> other_accounts = {HIVE_MINER_ACCOUNT, HIVE_NULL_ACCOUNT, HIVE_TEMP_ACCOUNT};
    for (const auto& account_name : other_accounts)
    {
        collected_keyauth_t default_item {account_name, key_t::MEMO, 0, true, public_key_type(), {}, 0};
        add_key_authorizations(collector, default_item, {key_t::MEMO});
    }

    return collector.collected_keyauths;
}

collected_keyauth_collection_t operation_get_hf09_keyauths()
{
    keyauth_collector collector;

    // Key types for HF09 accounts (no MEMO)
    std::vector<key_t> hf09_key_types = {key_t::OWNER, key_t::ACTIVE, key_t::POSTING};

    auto PUBLIC_KEY = public_key_type(HIVE_HF_9_COMPROMISED_ACCOUNTS_PUBLIC_KEY_STR);
    for (const std::string& ACCOUNT_NAME : hardfork9::get_compromised_accounts()) 
    {
        collected_keyauth_t hf09_item {ACCOUNT_NAME, key_t::OWNER, 1, true, PUBLIC_KEY, {}, 1};
        add_key_authorizations(collector, hf09_item, hf09_key_types);
    }

    return collector.collected_keyauths;
}

} // namespace hive::app

