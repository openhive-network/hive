#pragma once
#include <hive/chain/hive_fwd.hpp>

#include <hive/protocol/operations.hpp>
#include <hive/protocol/dhf_operations.hpp>

#include <hive/chain/witness_objects.hpp>

#include <hive/protocol/asset.hpp>

#include <fc/exception/exception.hpp>

namespace hive { namespace plugins { namespace condenser_api {

  template< typename T >
  struct convert_to_legacy_static_variant
  {
    convert_to_legacy_static_variant( T& l_sv ) :
      legacy_sv( l_sv ) {}

    T& legacy_sv;

    typedef void result_type;

    template< typename V >
    void operator()( const V& v ) const
    {
      legacy_sv = v;
    }
  };

  typedef static_variant<
        protocol::comment_payout_beneficiaries
  #ifdef HIVE_ENABLE_SMT
        ,protocol::allowed_vote_assets
  #endif
      > legacy_comment_options_extensions;

  typedef vector< legacy_comment_options_extensions > legacy_comment_options_extensions_type;

  typedef static_variant<
        protocol::pow2,
        protocol::equihash_pow
      > legacy_pow2_work;

  using namespace hive::protocol;

  typedef account_update_operation               legacy_account_update_operation;
  typedef account_update2_operation              legacy_account_update2_operation;
  typedef comment_operation                      legacy_comment_operation;
  typedef create_claimed_account_operation       legacy_create_claimed_account_operation;
  typedef delete_comment_operation               legacy_delete_comment_operation;
  typedef vote_operation                         legacy_vote_operation;
  typedef escrow_approve_operation               legacy_escrow_approve_operation;
  typedef escrow_dispute_operation               legacy_escrow_dispute_operation;
  typedef set_withdraw_vesting_route_operation   legacy_set_withdraw_vesting_route_operation;
  typedef witness_set_properties_operation       legacy_witness_set_properties_operation;
  typedef account_witness_vote_operation         legacy_account_witness_vote_operation;
  typedef account_witness_proxy_operation        legacy_account_witness_proxy_operation;
  typedef custom_operation                       legacy_custom_operation;
  typedef custom_json_operation                  legacy_custom_json_operation;
  typedef custom_binary_operation                legacy_custom_binary_operation;
  typedef limit_order_cancel_operation           legacy_limit_order_cancel_operation;
  typedef pow_operation                          legacy_pow_operation;
  typedef report_over_production_operation       legacy_report_over_production_operation;
  typedef request_account_recovery_operation     legacy_request_account_recovery_operation;
  typedef recover_account_operation              legacy_recover_account_operation;
  typedef reset_account_operation                legacy_reset_account_operation;
  typedef set_reset_account_operation            legacy_set_reset_account_operation;
  typedef change_recovery_account_operation      legacy_change_recovery_account_operation;
  typedef cancel_transfer_from_savings_operation legacy_cancel_transfer_from_savings_operation;
  typedef decline_voting_rights_operation        legacy_decline_voting_rights_operation;
  typedef shutdown_witness_operation             legacy_shutdown_witness_operation;
  typedef hardfork_operation                     legacy_hardfork_operation;
  typedef comment_payout_update_operation        legacy_comment_payout_update_operation;
  typedef update_proposal_votes_operation        legacy_update_proposal_votes_operation;
  typedef remove_proposal_operation              legacy_remove_proposal_operation;
  typedef clear_null_account_balance_operation   legacy_clear_null_account_balance_operation;
  typedef consolidate_treasury_balance_operation legacy_consolidate_treasury_balance_operation;
  typedef delayed_voting_operation               legacy_delayed_voting_operation;
  typedef expired_account_notification_operation legacy_expired_account_notification_operation;
  typedef changed_recovery_account_operation     legacy_changed_recovery_account_operation;
  typedef system_warning_operation               legacy_system_warning_operation;
  typedef ineffective_delete_comment_operation   legacy_ineffective_delete_comment_operation;
  typedef fill_recurrent_transfer_operation      legacy_fill_recurrent_transfer_operation;
  typedef failed_recurrent_transfer_operation    legacy_failed_recurrent_transfer_operation;
  typedef producer_missed_operation              legacy_producer_missed_operation;

  struct legacy_price
  {
    legacy_price() {}
    legacy_price( const protocol::price& p ) :
      base( legacy_asset::from_asset( p.base ) ),
      quote( legacy_asset::from_asset( p.quote ) )
    {}

    operator price()const { return price( base, quote ); }

    legacy_asset base;
    legacy_asset quote;
  };

  struct api_chain_properties
  {
    api_chain_properties() {}
    api_chain_properties( const hive::chain::chain_properties& c ) :
      account_creation_fee( legacy_asset::from_asset( c.account_creation_fee ) ),
      maximum_block_size( c.maximum_block_size ),
      hbd_interest_rate( c.hbd_interest_rate ),
      account_subsidy_budget( c.account_subsidy_budget ),
      account_subsidy_decay( c.account_subsidy_decay )
    {}

    operator legacy_chain_properties() const
    {
      legacy_chain_properties props;
      props.account_creation_fee = legacy_hive_asset::from_asset( asset( account_creation_fee ) );
      props.maximum_block_size = maximum_block_size;
      props.hbd_interest_rate = hbd_interest_rate;
      return props;
    }

    legacy_asset   account_creation_fee;
    uint32_t       maximum_block_size = HIVE_MIN_BLOCK_SIZE_LIMIT * 2;
    uint16_t       hbd_interest_rate = HIVE_DEFAULT_HBD_INTEREST_RATE;
    int32_t        account_subsidy_budget = HIVE_DEFAULT_ACCOUNT_SUBSIDY_BUDGET;
    uint32_t       account_subsidy_decay = HIVE_DEFAULT_ACCOUNT_SUBSIDY_DECAY;
  };

  struct legacy_account_create_operation
  {
    legacy_account_create_operation() {}
    legacy_account_create_operation( const account_create_operation& op ) :
      fee( legacy_asset::from_asset( op.fee ) ),
      creator( op.creator ),
      new_account_name( op.new_account_name ),
      owner( op.owner ),
      active( op.active ),
      posting( op.posting ),
      memo_key( op.memo_key ),
      json_metadata( op.json_metadata )
    {}

    operator account_create_operation()const
    {
      account_create_operation op;
      op.fee = fee;
      op.creator = creator;
      op.new_account_name = new_account_name;
      op.owner = owner;
      op.active = active;
      op.posting = posting;
      op.memo_key = memo_key;
      op.json_metadata = json_metadata;
      return op;
    }

    legacy_asset      fee;
    account_name_type creator;
    account_name_type new_account_name;
    authority         owner;
    authority         active;
    authority         posting;
    public_key_type   memo_key;
    string            json_metadata;
  };

  struct legacy_account_created_operation
  {
    legacy_account_created_operation() {}
    legacy_account_created_operation( const account_created_operation& op ) :
      new_account_name( op.new_account_name ),
      creator( op.creator ),
      initial_vesting_shares(legacy_asset::from_asset( op.initial_vesting_shares )),
      initial_delegation(legacy_asset::from_asset( op.initial_delegation ))
    {
      FC_ASSERT(creator.size(), "Every account should have a creator");
    }

    operator account_created_operation()const
    {
      account_created_operation op;
      op.new_account_name = new_account_name;
      op.creator = creator;
      op.initial_vesting_shares = initial_vesting_shares;
      op.initial_delegation = initial_delegation;
      return op;
    }

    account_name_type new_account_name;
    account_name_type creator;
    legacy_asset initial_vesting_shares;
    legacy_asset initial_delegation;
  };


  struct legacy_account_create_with_delegation_operation
  {
    legacy_account_create_with_delegation_operation() {}
    legacy_account_create_with_delegation_operation( const account_create_with_delegation_operation op ) :
      fee( legacy_asset::from_asset( op.fee ) ),
      delegation( legacy_asset::from_asset( op.delegation ) ),
      creator( op.creator ),
      new_account_name( op.new_account_name ),
      owner( op.owner ),
      active( op.active ),
      posting( op.posting ),
      memo_key( op.memo_key ),
      json_metadata( op.json_metadata )
    {
      extensions.insert( op.extensions.begin(), op.extensions.end() );
    }

    operator account_create_with_delegation_operation()const
    {
      account_create_with_delegation_operation op;
      op.fee = fee;
      op.delegation = delegation;
      op.creator = creator;
      op.new_account_name = new_account_name;
      op.owner = owner;
      op.active = active;
      op.posting = posting;
      op.memo_key = memo_key;
      op.json_metadata = json_metadata;
      op.extensions.insert( extensions.begin(), extensions.end() );
      return op;
    }

    legacy_asset      fee;
    legacy_asset      delegation;
    account_name_type creator;
    account_name_type new_account_name;
    authority         owner;
    authority         active;
    authority         posting;
    public_key_type   memo_key;
    string            json_metadata;

    extensions_type   extensions;
  };

  struct legacy_comment_options_operation
  {
    legacy_comment_options_operation() {}
    legacy_comment_options_operation( const comment_options_operation& op ) :
      author( op.author ),
      permlink( op.permlink ),
      max_accepted_payout( legacy_asset::from_asset( op.max_accepted_payout ) ),
      percent_hbd( op.percent_hbd ),
      allow_votes( op.allow_votes ),
      allow_curation_rewards( op.allow_curation_rewards )
    {
      for( const auto& e : op.extensions )
      {
        legacy_comment_options_extensions ext;
        e.visit( convert_to_legacy_static_variant< legacy_comment_options_extensions >( ext ) );
        extensions.push_back( e );
      }
    }

    operator comment_options_operation()const
    {
      comment_options_operation op;
      op.author = author;
      op.permlink = permlink;
      op.max_accepted_payout = max_accepted_payout;
      op.percent_hbd = percent_hbd;
      op.allow_curation_rewards = allow_curation_rewards;
      op.extensions.insert( extensions.begin(), extensions.end() );
      return op;
    }

    account_name_type author;
    string            permlink;

    legacy_asset      max_accepted_payout;
    uint16_t          percent_hbd;
    bool              allow_votes;
    bool              allow_curation_rewards;
    legacy_comment_options_extensions_type extensions;
  };


  struct legacy_transfer_operation
  {
    legacy_transfer_operation() {}
    legacy_transfer_operation( const transfer_operation& op ) :
      from( op.from ),
      to( op.to ),
      amount( legacy_asset::from_asset( op.amount ) ),
      memo( op.memo )
    {}

    operator transfer_operation()const
    {
      transfer_operation op;
      op.from = from;
      op.to = to;
      op.amount = amount;
      op.memo = memo;
      return op;
    }

    account_name_type from;
    account_name_type to;
    legacy_asset      amount;
    string            memo;
  };

  struct legacy_escrow_transfer_operation
  {
    legacy_escrow_transfer_operation() {}
    legacy_escrow_transfer_operation( const escrow_transfer_operation& op ) :
      from( op.from ),
      to( op.to ),
      agent( op.agent ),
      escrow_id( op.escrow_id ),
      hbd_amount( legacy_asset::from_asset( op.hbd_amount ) ),
      hive_amount( legacy_asset::from_asset( op.hive_amount ) ),
      fee( legacy_asset::from_asset( op.fee ) ),
      ratification_deadline( op.ratification_deadline ),
      escrow_expiration( op.escrow_expiration ),
      json_meta( op.json_meta )
    {}

    operator escrow_transfer_operation()const
    {
      escrow_transfer_operation op;
      op.from = from;
      op.to = to;
      op.agent = agent;
      op.escrow_id = escrow_id;
      op.hbd_amount = hbd_amount;
      op.hive_amount = hive_amount;
      op.fee = fee;
      op.ratification_deadline = ratification_deadline;
      op.escrow_expiration = escrow_expiration;
      op.json_meta = json_meta;
      return op;
    }

    account_name_type from;
    account_name_type to;
    account_name_type agent;
    uint32_t          escrow_id;

    legacy_asset      hbd_amount;
    legacy_asset      hive_amount;
    legacy_asset      fee;

    time_point_sec    ratification_deadline;
    time_point_sec    escrow_expiration;

    string            json_meta;
  };

  struct legacy_escrow_release_operation
  {
    legacy_escrow_release_operation() {}
    legacy_escrow_release_operation( const escrow_release_operation& op ) :
      from( op.from ),
      to( op.to ),
      agent( op.agent ),
      who( op.who ),
      receiver( op.receiver ),
      escrow_id( op.escrow_id ),
      hbd_amount( legacy_asset::from_asset( op.hbd_amount ) ),
      hive_amount( legacy_asset::from_asset( op.hive_amount ) )
    {}

    operator escrow_release_operation()const
    {
      escrow_release_operation op;
      op.from = from;
      op.to = to;
      op.agent = agent;
      op.who = who;
      op.receiver = receiver;
      op.escrow_id = escrow_id;
      op.hbd_amount = hbd_amount;
      op.hive_amount = hive_amount;
      return op;
    }

    account_name_type from;
    account_name_type to;
    account_name_type agent;
    account_name_type who;
    account_name_type receiver;

    uint32_t          escrow_id;
    legacy_asset      hbd_amount;
    legacy_asset      hive_amount;
  };

  struct legacy_pow2_operation
  {
    legacy_pow2_operation() {}
    legacy_pow2_operation( const pow2_operation& op ) :
      new_owner_key( op.new_owner_key )
    {
      op.work.visit( convert_to_legacy_static_variant< legacy_pow2_work >( work ) );
      props.account_creation_fee = legacy_asset::from_asset( op.props.account_creation_fee.to_asset< false >() );
      props.maximum_block_size = op.props.maximum_block_size;
      props.hbd_interest_rate = op.props.hbd_interest_rate;
    }

    operator pow2_operation()const
    {
      pow2_operation op;
      work.visit( convert_to_legacy_static_variant< pow2_work >( op.work ) );
      op.new_owner_key = new_owner_key;
      op.props.account_creation_fee = legacy_hive_asset::from_asset( asset( props.account_creation_fee ) );
      op.props.maximum_block_size = props.maximum_block_size;
      op.props.hbd_interest_rate = props.hbd_interest_rate;
      return op;
    }

    legacy_pow2_work              work;
    optional< public_key_type >   new_owner_key;
    api_chain_properties          props;
  };

  struct legacy_transfer_to_vesting_operation
  {
    legacy_transfer_to_vesting_operation() {}
    legacy_transfer_to_vesting_operation( const transfer_to_vesting_operation& op ) :
      from( op.from ),
      to( op.to ),
      amount( legacy_asset::from_asset( op.amount ) )
    {}

    operator transfer_to_vesting_operation()const
    {
      transfer_to_vesting_operation op;
      op.from = from;
      op.to = to;
      op.amount = amount;
      return op;
    }

    account_name_type from;
    account_name_type to;
    legacy_asset      amount;
  };

  struct legacy_transfer_to_vesting_completed_operation
  {
    legacy_transfer_to_vesting_completed_operation() {}
    legacy_transfer_to_vesting_completed_operation( const transfer_to_vesting_completed_operation& op ) :
      from_account(op.from_account),
      to_account(op.to_account),
      hive_vested(legacy_asset::from_asset( op.hive_vested )),
      vesting_shares_received(legacy_asset::from_asset( op.vesting_shares_received ))
    {}

    operator transfer_to_vesting_completed_operation()const
    {
      transfer_to_vesting_completed_operation op;
      op.from_account = from_account;
      op.to_account = to_account;
      op.hive_vested = hive_vested;
      op.vesting_shares_received = vesting_shares_received;
      return op;
    }

    account_name_type from_account;
    account_name_type to_account;
    legacy_asset      hive_vested;
    legacy_asset      vesting_shares_received;
  };

  struct legacy_create_proposal_operation
  {
    legacy_create_proposal_operation() {}
    legacy_create_proposal_operation( const create_proposal_operation& op ) :
      creator( op.creator ),
      receiver( op.receiver ),
      start_date( op.start_date ),
      end_date( op.end_date ),
      daily_pay( legacy_asset::from_asset( op.daily_pay ) ),
      subject( op.subject ),
      permlink( op.permlink)
    {}

    operator create_proposal_operation()const
    {
      create_proposal_operation op;
      op.creator = creator;
      op.receiver = receiver;
      op.start_date = start_date;
      op.end_date = end_date;
      op.daily_pay = daily_pay;
      op.subject = subject;
      op.permlink = permlink;
      return op;
    }

    account_name_type creator;
    account_name_type receiver;
    time_point_sec start_date;
    time_point_sec end_date;
    legacy_asset daily_pay;
    string subject;
    string permlink;
  };

  struct legacy_update_proposal_operation
  {
    legacy_update_proposal_operation() {}
    legacy_update_proposal_operation( const update_proposal_operation& op ) :
      proposal_id( op.proposal_id),
      creator( op.creator ),
      daily_pay( legacy_asset::from_asset( op.daily_pay ) ),
      subject( op.subject ),
      permlink( op.permlink)
    {
      extensions.insert( op.extensions.begin(), op.extensions.end() );
    }

    operator update_proposal_operation()const
    {
      update_proposal_operation op;
      op.proposal_id = proposal_id;
      op.creator = creator;
      op.daily_pay = daily_pay;
      op.subject = subject;
      op.permlink = permlink;
      op.extensions.insert( extensions.begin(), extensions.end() );
      return op;
    }

    uint64_t proposal_id;
    account_name_type creator;
    legacy_asset daily_pay;
    string subject;
    string permlink;
    hive::protocol::update_proposal_extensions_type extensions;
  };

  struct legacy_withdraw_vesting_operation
  {
    legacy_withdraw_vesting_operation() {}
    legacy_withdraw_vesting_operation( const withdraw_vesting_operation& op ) :
      account( op.account ),
      vesting_shares( legacy_asset::from_asset( op.vesting_shares) )
    {}

    operator withdraw_vesting_operation()const
    {
      withdraw_vesting_operation op;
      op.account = account;
      op.vesting_shares = vesting_shares;
      return op;
    }

    account_name_type account;
    legacy_asset      vesting_shares;
  };

  struct legacy_witness_update_operation
  {
    legacy_witness_update_operation() {}
    legacy_witness_update_operation( const witness_update_operation& op ) :
      owner( op.owner ),
      url( op.url ),
      block_signing_key( op.block_signing_key ),
      fee( legacy_asset::from_asset( op.fee ) )
    {
      props.account_creation_fee = legacy_asset::from_asset( op.props.account_creation_fee.to_asset< false >() );
      props.maximum_block_size = op.props.maximum_block_size;
      props.hbd_interest_rate = op.props.hbd_interest_rate;
    }

    operator witness_update_operation()const
    {
      witness_update_operation op;
      op.owner = owner;
      op.url = url;
      op.block_signing_key = block_signing_key;
      op.props.account_creation_fee = legacy_hive_asset::from_asset( asset( props.account_creation_fee ) );
      op.props.maximum_block_size = props.maximum_block_size;
      op.props.hbd_interest_rate = props.hbd_interest_rate;
      op.fee = fee;
      return op;
    }

    account_name_type       owner;
    string                  url;
    public_key_type         block_signing_key;
    api_chain_properties    props;
    legacy_asset            fee;
  };

  struct legacy_feed_publish_operation
  {
    legacy_feed_publish_operation() {}
    legacy_feed_publish_operation( const feed_publish_operation& op ) :
      publisher( op.publisher ),
      exchange_rate( legacy_price( op.exchange_rate ) )
    {}

    operator feed_publish_operation()const
    {
      feed_publish_operation op;
      op.publisher = publisher;
      op.exchange_rate = exchange_rate;
      return op;
    }

    account_name_type publisher;
    legacy_price      exchange_rate;
  };

  struct legacy_convert_operation
  {
    legacy_convert_operation() {}
    legacy_convert_operation( const convert_operation& op ) :
      owner( op.owner ),
      requestid( op.requestid ),
      amount( legacy_asset::from_asset( op.amount ) )
    {}

    operator convert_operation()const
    {
      convert_operation op;
      op.owner = owner;
      op.requestid = requestid;
      op.amount = amount;
      return op;
    }

    account_name_type owner;
    uint32_t          requestid = 0;
    legacy_asset      amount;
  };

  struct legacy_collateralized_convert_operation
  {
    legacy_collateralized_convert_operation() {}
    legacy_collateralized_convert_operation( const collateralized_convert_operation& op ) :
      owner( op.owner ),
      requestid( op.requestid ),
      amount( legacy_asset::from_asset( op.amount ) )
    {}

    operator collateralized_convert_operation()const
    {
      collateralized_convert_operation op;
      op.owner = owner;
      op.requestid = requestid;
      op.amount = amount;
      return op;
    }

    account_name_type owner;
    uint32_t          requestid = 0;
    legacy_asset      amount;
  };

  struct legacy_limit_order_create_operation
  {
    legacy_limit_order_create_operation() {}
    legacy_limit_order_create_operation( const limit_order_create_operation& op ) :
      owner( op.owner ),
      orderid( op.orderid ),
      amount_to_sell( legacy_asset::from_asset( op.amount_to_sell ) ),
      min_to_receive( legacy_asset::from_asset( op.min_to_receive ) ),
      fill_or_kill( op.fill_or_kill ),
      expiration( op.expiration )
    {}

    operator limit_order_create_operation()const
    {
      limit_order_create_operation op;
      op.owner = owner;
      op.orderid = orderid;
      op.amount_to_sell = amount_to_sell;
      op.min_to_receive = min_to_receive;
      op.fill_or_kill = fill_or_kill;
      op.expiration = expiration;
      return op;
    }

    account_name_type owner;
    uint32_t          orderid;
    legacy_asset      amount_to_sell;
    legacy_asset      min_to_receive;
    bool              fill_or_kill;
    time_point_sec    expiration;
  };

  struct legacy_limit_order_create2_operation
  {
    legacy_limit_order_create2_operation() {}
    legacy_limit_order_create2_operation( const limit_order_create2_operation& op ) :
      owner( op.owner ),
      orderid( op.orderid ),
      amount_to_sell( legacy_asset::from_asset( op.amount_to_sell ) ),
      fill_or_kill( op.fill_or_kill ),
      exchange_rate( op.exchange_rate ),
      expiration( op.expiration )
    {}

    operator limit_order_create2_operation()const
    {
      limit_order_create2_operation op;
      op.owner = owner;
      op.orderid = orderid;
      op.amount_to_sell = amount_to_sell;
      op.fill_or_kill = fill_or_kill;
      op.exchange_rate = exchange_rate;
      op.expiration = expiration;
      return op;
    }

    account_name_type owner;
    uint32_t          orderid;
    legacy_asset      amount_to_sell;
    bool              fill_or_kill;
    legacy_price      exchange_rate;
    time_point_sec    expiration;
  };

  struct legacy_transfer_to_savings_operation
  {
    legacy_transfer_to_savings_operation() {}
    legacy_transfer_to_savings_operation( const transfer_to_savings_operation& op ) :
      from( op.from ),
      to( op.to ),
      amount( legacy_asset::from_asset( op.amount ) ),
      memo( op.memo )
    {}

    operator transfer_to_savings_operation()const
    {
      transfer_to_savings_operation op;
      op.from = from;
      op.to = to;
      op.amount = amount;
      op.memo = memo;
      return op;
    }

    account_name_type from;
    account_name_type to;
    legacy_asset      amount;
    string            memo;
  };

  struct legacy_transfer_from_savings_operation
  {
    legacy_transfer_from_savings_operation() {}
    legacy_transfer_from_savings_operation( const transfer_from_savings_operation& op ) :
      from( op.from ),
      request_id( op.request_id ),
      to( op.to ),
      amount( legacy_asset::from_asset( op.amount ) ),
      memo( op.memo )
    {}

    operator transfer_from_savings_operation()const
    {
      transfer_from_savings_operation op;
      op.from = from;
      op.request_id = request_id;
      op.to = to;
      op.amount = amount;
      op.memo = memo;
      return op;
    }

    account_name_type from;
    uint32_t          request_id;
    account_name_type to;
    legacy_asset      amount;
    string            memo;
  };

  struct legacy_claim_reward_balance_operation
  {
    legacy_claim_reward_balance_operation() {}
    legacy_claim_reward_balance_operation( const claim_reward_balance_operation& op ) :
      account( op.account ),
      reward_hive( legacy_asset::from_asset( op.reward_hive ) ),
      reward_hbd( legacy_asset::from_asset( op.reward_hbd ) ),
      reward_vests( legacy_asset::from_asset( op.reward_vests ) )
    {}

    operator claim_reward_balance_operation()const
    {
      claim_reward_balance_operation op;
      op.account = account;
      op.reward_hive = reward_hive;
      op.reward_hbd = reward_hbd;
      op.reward_vests = reward_vests;
      return op;
    }

    account_name_type account;
    legacy_asset      reward_hive;
    legacy_asset      reward_hbd;
    legacy_asset      reward_vests;
  };

  struct legacy_delegate_vesting_shares_operation
  {
    legacy_delegate_vesting_shares_operation() {}
    legacy_delegate_vesting_shares_operation( const delegate_vesting_shares_operation& op ) :
      delegator( op.delegator ),
      delegatee( op.delegatee ),
      vesting_shares( legacy_asset::from_asset( op.vesting_shares ) )
    {}

    operator delegate_vesting_shares_operation()const
    {
      delegate_vesting_shares_operation op;
      op.delegator = delegator;
      op.delegatee = delegatee;
      op.vesting_shares = vesting_shares;
      return op;
    }

    account_name_type delegator;
    account_name_type delegatee;
    legacy_asset      vesting_shares;
  };

  struct legacy_author_reward_operation
  {
    legacy_author_reward_operation() = default;
    explicit legacy_author_reward_operation( const author_reward_operation& op ) :
      author( op.author ),
      permlink( op.permlink ),
      hbd_payout( legacy_asset::from_asset( op.hbd_payout ) ),
      hive_payout( legacy_asset::from_asset( op.hive_payout ) ),
      vesting_payout( legacy_asset::from_asset( op.vesting_payout ) ),
      payout_must_be_claimed( op.payout_must_be_claimed)
    {}

    operator author_reward_operation()const
    {
      author_reward_operation op;
      op.author = author;
      op.permlink = permlink;
      op.hbd_payout = hbd_payout;
      op.hive_payout = hive_payout;
      op.vesting_payout = vesting_payout;
      op.payout_must_be_claimed = payout_must_be_claimed;
      return op;
    }

    account_name_type author;
    string            permlink;
    legacy_asset      hbd_payout;
    legacy_asset      hive_payout;
    legacy_asset      vesting_payout;
    bool              payout_must_be_claimed = false;
  };

  struct legacy_curation_reward_operation
  {
    legacy_curation_reward_operation() = default;
    explicit legacy_curation_reward_operation( const curation_reward_operation& op ) :
      curator( op.curator ),
      reward( legacy_asset::from_asset( op.reward ) ),
      comment_author( op.comment_author ),
      comment_permlink( op.comment_permlink ),
      payout_must_be_claimed( op.payout_must_be_claimed)
    {}

    operator curation_reward_operation()const
    {
      curation_reward_operation op;
      op.curator = curator;
      op.reward = reward;
      op.comment_author = comment_author;
      op.comment_permlink = comment_permlink;
      op.payout_must_be_claimed = payout_must_be_claimed;
      return op;
    }

    account_name_type curator;
    legacy_asset      reward;
    account_name_type comment_author;
    string            comment_permlink;
    bool              payout_must_be_claimed = false;
  };

  struct legacy_comment_reward_operation
  {
    legacy_comment_reward_operation() {}
    legacy_comment_reward_operation( const comment_reward_operation& op ) :
      author( op.author ),
      permlink( op.permlink ),
      payout( legacy_asset::from_asset( op.payout ) )
    {}

    operator comment_reward_operation()const
    {
      comment_reward_operation op;
      op.author = author;
      op.permlink = permlink;
      op.payout = payout;
      return op;
    }

    account_name_type author;
    string            permlink;
    legacy_asset      payout;
  };

  struct legacy_liquidity_reward_operation
  {
    legacy_liquidity_reward_operation() {}
    legacy_liquidity_reward_operation( const liquidity_reward_operation& op ) :
      owner( op.owner ),
      payout( legacy_asset::from_asset( op.payout ) )
    {}

    operator liquidity_reward_operation()const
    {
      liquidity_reward_operation op;
      op.owner = owner;
      op.payout = payout;
      return op;
    }

    account_name_type owner;
    legacy_asset      payout;
  };

  struct legacy_interest_operation
  {
    legacy_interest_operation() {}
    legacy_interest_operation( const interest_operation& op ) :
      owner( op.owner ),
      interest( legacy_asset::from_asset( op.interest ) )
    {}

    operator interest_operation()const
    {
      interest_operation op;
      op.owner = owner;
      op.interest = interest;
      return op;
    }

    account_name_type owner;
    legacy_asset      interest;
  };

  struct legacy_fill_convert_request_operation
  {
    legacy_fill_convert_request_operation() {}
    legacy_fill_convert_request_operation( const fill_convert_request_operation& op ) :
      owner( op.owner ),
      requestid( op.requestid ),
      amount_in( legacy_asset::from_asset( op.amount_in ) ),
      amount_out( legacy_asset::from_asset( op.amount_out ) )
    {}

    operator fill_convert_request_operation()const
    {
      fill_convert_request_operation op;
      op.owner = owner;
      op.requestid = requestid;
      op.amount_in = amount_in;
      op.amount_out = amount_out;
      return op;
    }

    account_name_type owner;
    uint32_t          requestid;
    legacy_asset      amount_in;
    legacy_asset      amount_out;
  };

  struct legacy_fill_collateralized_convert_request_operation
  {
    legacy_fill_collateralized_convert_request_operation() {}
    legacy_fill_collateralized_convert_request_operation( const fill_collateralized_convert_request_operation& op ) :
      owner( op.owner ),
      requestid( op.requestid ),
      amount_in( legacy_asset::from_asset( op.amount_in ) ),
      amount_out( legacy_asset::from_asset( op.amount_out ) ),
      excess_collateral( legacy_asset::from_asset( op.excess_collateral ) )
    {}

    operator fill_collateralized_convert_request_operation()const
    {
      fill_collateralized_convert_request_operation op;
      op.owner = owner;
      op.requestid = requestid;
      op.amount_in = amount_in;
      op.amount_out = amount_out;
      op.excess_collateral = excess_collateral;
      return op;
    }

    account_name_type owner;
    uint32_t          requestid;
    legacy_asset      amount_in;
    legacy_asset      amount_out;
    legacy_asset      excess_collateral;
  };

  struct legacy_fill_vesting_withdraw_operation
  {
    legacy_fill_vesting_withdraw_operation() {}
    legacy_fill_vesting_withdraw_operation( const fill_vesting_withdraw_operation& op ) :
      from_account( op.from_account ),
      to_account( op.to_account ),
      withdrawn( legacy_asset::from_asset( op.withdrawn ) ),
      deposited( legacy_asset::from_asset( op.deposited ) )
    {}

    operator fill_vesting_withdraw_operation()const
    {
      fill_vesting_withdraw_operation op;
      op.from_account = from_account;
      op.to_account = to_account;
      op.withdrawn = withdrawn;
      op.deposited = deposited;
      return op;
    }

    account_name_type from_account;
    account_name_type to_account;
    legacy_asset      withdrawn;
    legacy_asset      deposited;
  };

  struct legacy_fill_order_operation
  {
    legacy_fill_order_operation() {}
    legacy_fill_order_operation( const fill_order_operation& op ) :
      current_owner( op.current_owner ),
      current_orderid( op.current_orderid ),
      current_pays( legacy_asset::from_asset( op.current_pays ) ),
      open_owner( op.open_owner ),
      open_orderid( op.open_orderid ),
      open_pays( legacy_asset::from_asset( op.open_pays ) )
    {}

    operator fill_order_operation()const
    {
      fill_order_operation op;
      op.current_owner = current_owner;
      op.current_orderid = current_orderid;
      op.current_pays = current_pays;
      op.open_owner = open_owner;
      op.open_orderid = open_orderid;
      op.open_pays = open_pays;
      return op;
    }

    account_name_type current_owner;
    uint32_t          current_orderid;
    legacy_asset      current_pays;
    account_name_type open_owner;
    uint32_t          open_orderid;
    legacy_asset      open_pays;
  };

  struct legacy_limit_order_cancelled_operation
  {
    legacy_limit_order_cancelled_operation() {}
    legacy_limit_order_cancelled_operation( const limit_order_cancelled_operation& op ) :
      seller( op.seller ),
      orderid( op.orderid ),
      amount_back( legacy_asset::from_asset( op.amount_back ) )
    {}

    operator limit_order_cancelled_operation()const
    {
      limit_order_cancelled_operation op;
      op.seller = seller;
      op.orderid = orderid;
      op.amount_back = amount_back;
      return op;
    }

    account_name_type seller;
    uint32_t          orderid = 0;
    legacy_asset      amount_back;
  };

  struct legacy_fill_transfer_from_savings_operation
  {
    legacy_fill_transfer_from_savings_operation() {}
    legacy_fill_transfer_from_savings_operation( const fill_transfer_from_savings_operation& op ) :
      from( op.from ),
      to( op.to ),
      amount( legacy_asset::from_asset( op.amount ) ),
      request_id( op.request_id ),
      memo( op.memo )
    {}

    operator fill_transfer_from_savings_operation()const
    {
      fill_transfer_from_savings_operation op;
      op.from = from;
      op.to = to;
      op.amount = amount;
      op.request_id = request_id;
      op.memo = memo;
      return op;
    }

    account_name_type from;
    account_name_type to;
    legacy_asset      amount;
    uint32_t          request_id = 0;
    string            memo;
  };

  struct legacy_return_vesting_delegation_operation
  {
    legacy_return_vesting_delegation_operation() {}
    legacy_return_vesting_delegation_operation( const return_vesting_delegation_operation& op ) :
      account( op.account ),
      vesting_shares( legacy_asset::from_asset( op.vesting_shares ) )
    {}

    operator return_vesting_delegation_operation()const
    {
      return_vesting_delegation_operation op;
      op.account = account;
      op.vesting_shares = vesting_shares;
      return op;
    }

    account_name_type account;
    legacy_asset      vesting_shares;
  };

  struct legacy_comment_benefactor_reward_operation
  {
    legacy_comment_benefactor_reward_operation() {}
    legacy_comment_benefactor_reward_operation( const comment_benefactor_reward_operation& op ) :
      benefactor( op.benefactor ),
      author( op.author ),
      permlink( op.permlink ),
      hbd_payout( legacy_asset::from_asset( op.hbd_payout ) ),
      hive_payout( legacy_asset::from_asset( op.hive_payout ) ),
      vesting_payout( legacy_asset::from_asset( op.vesting_payout ) ),
      payout_must_be_claimed( op.payout_must_be_claimed )
    {}

    operator comment_benefactor_reward_operation()const
    {
      comment_benefactor_reward_operation op;
      op.benefactor = benefactor;
      op.author = author;
      op.permlink = permlink;
      op.hbd_payout = hbd_payout;
      op.hive_payout = hive_payout;
      op.vesting_payout = vesting_payout;
      op.payout_must_be_claimed = payout_must_be_claimed;
      return op;
    }

    account_name_type benefactor;
    account_name_type author;
    string            permlink;
    legacy_asset      hbd_payout;
    legacy_asset      hive_payout;
    legacy_asset      vesting_payout;
    bool              payout_must_be_claimed = false;
  };

  struct legacy_producer_reward_operation
  {
    legacy_producer_reward_operation() {}
    legacy_producer_reward_operation( const producer_reward_operation& op ) :
      producer( op.producer ),
      vesting_shares( legacy_asset::from_asset( op.vesting_shares ) )
    {}

    operator producer_reward_operation()const
    {
      producer_reward_operation op;
      op.producer = producer;
      op.vesting_shares = vesting_shares;
      return op;
    }

    account_name_type producer;
    legacy_asset      vesting_shares;
  };

  struct legacy_claim_account_operation
  {
    legacy_claim_account_operation() {}
    legacy_claim_account_operation( const claim_account_operation& op ) :
      creator( op.creator ),
      fee( legacy_asset::from_asset( op.fee ) )
    {
      extensions.insert( op.extensions.begin(), op.extensions.end() );
    }

    operator claim_account_operation()const
    {
      claim_account_operation op;
      op.creator = creator;
      op.fee = fee;
      op.extensions.insert( extensions.begin(), extensions.end() );
      return op;
    }

    account_name_type creator;
    legacy_asset      fee;
    extensions_type   extensions;
  };

  struct legacy_vesting_shares_split_operation
  {
    legacy_vesting_shares_split_operation() {}
    legacy_vesting_shares_split_operation( const vesting_shares_split_operation& op ) :
      owner( op.owner ),
      vesting_shares_before_split( legacy_asset::from_asset( op.vesting_shares_before_split ) ),
      vesting_shares_after_split( legacy_asset::from_asset( op.vesting_shares_after_split ) )
    {}

    operator vesting_shares_split_operation()const
    {
      vesting_shares_split_operation op;
      op.owner = owner;
      op.vesting_shares_before_split = vesting_shares_before_split;
      op.vesting_shares_after_split = vesting_shares_after_split;
      return op;
    }

    account_name_type owner;
    legacy_asset      vesting_shares_before_split;
    legacy_asset      vesting_shares_after_split;
  };

  struct legacy_pow_reward_operation
  {
    legacy_pow_reward_operation() {}
    legacy_pow_reward_operation( const pow_reward_operation& op ) :
      worker( op.worker ),
      reward( legacy_asset::from_asset( op.reward ) )
    {}

    operator pow_reward_operation()const
    {
      pow_reward_operation op;
      op.worker = worker;
      op.reward = reward;
      return op;
    }

    account_name_type worker;
    legacy_asset      reward;
  };

  struct legacy_proposal_pay_operation
  {
    legacy_proposal_pay_operation() {}
    legacy_proposal_pay_operation( const proposal_pay_operation& op ) :
      proposal_id( op.proposal_id ),
      receiver( op.receiver ),
      payer( op.payer ),
      payment( legacy_asset::from_asset( op.payment ) ),
      trx_id( op.trx_id ),
      op_in_trx( op.op_in_trx )
    {}

    operator proposal_pay_operation()const
    {
      proposal_pay_operation op;
      op.proposal_id = proposal_id;
      op.receiver = receiver;
      op.payer = payer;
      op.payment = payment;
      op.trx_id = trx_id;
      op.op_in_trx = op_in_trx;
      return op;
    }

    uint32_t             proposal_id = 0;
    account_name_type    receiver;
    account_name_type    payer;
    legacy_asset         payment;
    transaction_id_type  trx_id;
    uint16_t             op_in_trx = 0;
  };

  struct legacy_dhf_funding_operation
  {
    legacy_dhf_funding_operation() {}
    legacy_dhf_funding_operation( const dhf_funding_operation& op ) :
      treasury( op.treasury ), additional_funds( legacy_asset::from_asset( op.additional_funds ) )
    {}

    operator dhf_funding_operation()const
    {
      dhf_funding_operation op;
      op.treasury = treasury;
      op.additional_funds = additional_funds;
      return op;
    }

    account_name_type treasury;
    legacy_asset additional_funds;
  };

  struct legacy_hardfork_hive_operation
  {
    legacy_hardfork_hive_operation() {}
    legacy_hardfork_hive_operation( const hardfork_hive_operation& op ) :
      account( op.account ), treasury( op.treasury ), other_affected_accounts( op.other_affected_accounts ),
      hbd_transferred( legacy_asset::from_asset( op.hbd_transferred ) ),
      hive_transferred( legacy_asset::from_asset( op.hive_transferred ) ),
      vests_converted( legacy_asset::from_asset( op.vests_converted ) ),
      total_hive_from_vests( legacy_asset::from_asset( op.total_hive_from_vests ) )
    {}

    operator hardfork_hive_operation()const
    {
      hardfork_hive_operation op;
      op.account = account;
      op.treasury = treasury;
      op.other_affected_accounts = other_affected_accounts;
      op.hbd_transferred = hbd_transferred;
      op.hive_transferred = hive_transferred;
      op.vests_converted = vests_converted;
      op.total_hive_from_vests = total_hive_from_vests;
      return op;
    }

    account_name_type account;
    account_name_type treasury;
    std::vector< account_name_type >
                      other_affected_accounts;
    legacy_asset      hbd_transferred;
    legacy_asset      hive_transferred;
    legacy_asset      vests_converted;
    legacy_asset      total_hive_from_vests;
  };

  struct legacy_hardfork_hive_restore_operation
  {
    legacy_hardfork_hive_restore_operation() {}
    legacy_hardfork_hive_restore_operation( const hardfork_hive_restore_operation& op ) :
      account( op.account ), treasury( op.treasury ),
      hbd_transferred( legacy_asset::from_asset( op.hbd_transferred ) ),
      hive_transferred( legacy_asset::from_asset( op.hive_transferred ) )
    {}

    operator hardfork_hive_restore_operation()const
    {
      hardfork_hive_restore_operation op;
      op.account = account;
      op.treasury = treasury;
      op.hbd_transferred = hbd_transferred;
      op.hive_transferred = hive_transferred;
      return op;
    }

    account_name_type account;
    account_name_type treasury;
    legacy_asset      hbd_transferred;
    legacy_asset      hive_transferred;
  };

  struct legacy_effective_comment_vote_operation{
    legacy_effective_comment_vote_operation() {};
    legacy_effective_comment_vote_operation(const effective_comment_vote_operation& op) :
      voter( op.voter ), author( op.author ),
      permlink( op.permlink ), weight( op.weight ),
      rshares( op.rshares ), total_vote_weight( op.total_vote_weight ),
      pending_payout( legacy_asset::from_asset( op.pending_payout ) )
    {}

    operator effective_comment_vote_operation()const
    {
      effective_comment_vote_operation op;
      op.voter = voter;
      op.author = author;
      op.permlink = permlink;
      op.weight = weight;
      op.rshares = rshares;
      op.total_vote_weight = total_vote_weight;
      op.pending_payout = pending_payout;
      return op;
    }

    account_name_type voter;
    account_name_type author;
    string            permlink;
    uint64_t          weight = 0;
    int64_t           rshares = 0;
    uint64_t          total_vote_weight = 0;
    legacy_asset      pending_payout;
  };

  struct legacy_recurrent_transfer_operation
  {
    legacy_recurrent_transfer_operation() {}
    legacy_recurrent_transfer_operation( const recurrent_transfer_operation& op ) :
    from( op.from ),
    to( op.to ),
    amount( legacy_asset::from_asset( op.amount ) ),
    memo( op.memo ),
    recurrence( op.recurrence ),
    executions( op.executions )
    {}

    operator recurrent_transfer_operation()const
    {
      recurrent_transfer_operation op;
      op.from = from;
      op.to = to;
      op.amount = amount;
      op.memo = memo;
      op.recurrence = recurrence;
      op.executions = executions;
      return op;
    }

    account_name_type from;
    account_name_type to;
    legacy_asset      amount;
    string            memo;
    uint16_t          recurrence = 0;
    uint16_t          executions = 0;
  };

  struct legacy_dhf_conversion_operation
  {
    legacy_dhf_conversion_operation() {}
    legacy_dhf_conversion_operation( const dhf_conversion_operation& op ) :
      treasury( op.treasury ),
      hive_amount_in( legacy_asset::from_asset( op.hive_amount_in ) ),
      hbd_amount_out( legacy_asset::from_asset( op.hbd_amount_out ) )
    {}

    operator dhf_conversion_operation()const
    {
      dhf_conversion_operation op;
      op.treasury = treasury;
      op.hive_amount_in = hive_amount_in;
      op.hbd_amount_out = hbd_amount_out;
      return op;
    }

    account_name_type treasury;
    legacy_asset      hive_amount_in;
    legacy_asset      hbd_amount_out;
  };

  struct legacy_proposal_fee_operation
  {
    legacy_proposal_fee_operation() {}
    legacy_proposal_fee_operation( const proposal_fee_operation& op ) :
      creator( op.creator ),
      treasury( op.treasury ),
      proposal_id( op.proposal_id ),
      fee( legacy_asset::from_asset( op.fee ) )
    {}

    operator proposal_fee_operation()const
    {
      proposal_fee_operation op;
      op.creator = creator;
      op.treasury = treasury;
      op.proposal_id = proposal_id;
      op.fee = fee;
      return op;
    }

    account_name_type creator;
    account_name_type treasury;
    uint32_t          proposal_id = 0;
    legacy_asset      fee;
  };

  struct legacy_collateralized_convert_immediate_conversion_operation
  {
    legacy_collateralized_convert_immediate_conversion_operation() {}
    legacy_collateralized_convert_immediate_conversion_operation( const collateralized_convert_immediate_conversion_operation& op ) :
      owner( op.owner ),
      requestid( op.requestid ),
      hbd_out( legacy_asset::from_asset( op.hbd_out ) )
    {}

    operator collateralized_convert_immediate_conversion_operation()const
    {
      collateralized_convert_immediate_conversion_operation op;
      op.owner = owner;
      op.requestid = requestid;
      op.hbd_out = hbd_out;
      return op;
    }

    account_name_type owner;
    uint32_t requestid = 0;
    legacy_asset hbd_out;
  };

  struct legacy_escrow_approved_operation
  {
    legacy_escrow_approved_operation() {}
    legacy_escrow_approved_operation( const escrow_approved_operation& op ) :
      from( op.from ),
      to( op.to ),
      agent( op.agent ),
      escrow_id( op.escrow_id ),
      fee( legacy_asset::from_asset( op.fee ) )
    {}

    operator escrow_approved_operation()const
    {
      escrow_approved_operation op;
      op.from = from;
      op.to = to;
      op.agent = agent;
      op.escrow_id = escrow_id;
      op.fee = fee;
      return op;
    }

    account_name_type from;
    account_name_type to;
    account_name_type agent;
    uint32_t escrow_id;
    legacy_asset fee;
  };

  struct legacy_escrow_rejected_operation
  {
    legacy_escrow_rejected_operation() {}
    legacy_escrow_rejected_operation( const escrow_rejected_operation& op ) :
      from( op.from ),
      to( op.to ),
      agent( op.agent ),
      escrow_id( op.escrow_id ),
      hbd_amount( legacy_asset::from_asset( op.hbd_amount ) ),
      hive_amount( legacy_asset::from_asset( op.hive_amount ) ),
      fee( legacy_asset::from_asset( op.fee ) )
    {}

    operator escrow_rejected_operation()const
    {
      escrow_rejected_operation op;
      op.from = from;
      op.to = to;
      op.agent = agent;
      op.escrow_id = escrow_id;
      op.hbd_amount = hbd_amount;
      op.hive_amount = hive_amount;
      op.fee = fee;
      return op;
    }

    account_name_type from;
    account_name_type to;
    account_name_type agent;
    uint32_t escrow_id;
    legacy_asset hbd_amount;
    legacy_asset hive_amount;
    legacy_asset fee;
  };

  typedef fc::static_variant<
        legacy_vote_operation,
        legacy_comment_operation,
        legacy_transfer_operation,
        legacy_transfer_to_vesting_operation,
        legacy_withdraw_vesting_operation,
        legacy_limit_order_create_operation,
        legacy_limit_order_cancel_operation,
        legacy_feed_publish_operation,
        legacy_convert_operation,
        legacy_account_create_operation,
        legacy_account_update_operation,
        legacy_witness_update_operation,
        legacy_account_witness_vote_operation,
        legacy_account_witness_proxy_operation,
        legacy_pow_operation,
        legacy_custom_operation,
        legacy_report_over_production_operation,
        legacy_delete_comment_operation,
        legacy_custom_json_operation,
        legacy_comment_options_operation,
        legacy_set_withdraw_vesting_route_operation,
        legacy_limit_order_create2_operation,
        legacy_claim_account_operation,
        legacy_create_claimed_account_operation,
        legacy_request_account_recovery_operation,
        legacy_recover_account_operation,
        legacy_change_recovery_account_operation,
        legacy_escrow_transfer_operation,
        legacy_escrow_dispute_operation,
        legacy_escrow_release_operation,
        legacy_pow2_operation,
        legacy_escrow_approve_operation,
        legacy_transfer_to_savings_operation,
        legacy_transfer_from_savings_operation,
        legacy_cancel_transfer_from_savings_operation,
        legacy_custom_binary_operation,
        legacy_decline_voting_rights_operation,
        legacy_reset_account_operation,
        legacy_set_reset_account_operation,
        legacy_claim_reward_balance_operation,
        legacy_delegate_vesting_shares_operation,
        legacy_account_create_with_delegation_operation,
        legacy_witness_set_properties_operation,
        legacy_account_update2_operation,
        legacy_create_proposal_operation,
        legacy_update_proposal_votes_operation,
        legacy_remove_proposal_operation,
        legacy_update_proposal_operation,
        legacy_collateralized_convert_operation,
        legacy_fill_convert_request_operation,
        legacy_author_reward_operation,
        legacy_curation_reward_operation,
        legacy_comment_reward_operation,
        legacy_liquidity_reward_operation,
        legacy_interest_operation,
        legacy_fill_vesting_withdraw_operation,
        legacy_fill_order_operation,
        legacy_shutdown_witness_operation,
        legacy_fill_transfer_from_savings_operation,
        legacy_hardfork_operation,
        legacy_comment_payout_update_operation,
        legacy_return_vesting_delegation_operation,
        legacy_comment_benefactor_reward_operation,
        legacy_producer_reward_operation,
        legacy_clear_null_account_balance_operation,
        legacy_proposal_pay_operation,
        legacy_dhf_funding_operation,
        legacy_hardfork_hive_operation,
        legacy_hardfork_hive_restore_operation,
        legacy_delayed_voting_operation,
        legacy_consolidate_treasury_balance_operation,
        legacy_dhf_conversion_operation,
        legacy_expired_account_notification_operation,
        legacy_changed_recovery_account_operation,
        legacy_transfer_to_vesting_completed_operation,
        legacy_account_created_operation, //for unknown reason this and two below are in different order than in hive::protocol::operation
        legacy_vesting_shares_split_operation, //since the order is affecting external libraries
        legacy_pow_reward_operation, //I'm not sure it can be changed to proper one
        legacy_fill_collateralized_convert_request_operation,
        legacy_system_warning_operation,
        legacy_effective_comment_vote_operation,
        legacy_ineffective_delete_comment_operation,
        legacy_recurrent_transfer_operation,
        legacy_fill_recurrent_transfer_operation,
        legacy_failed_recurrent_transfer_operation,
        legacy_producer_missed_operation,
        legacy_limit_order_cancelled_operation, //was missing from the list (should be much earlier but see comment above)
        legacy_proposal_fee_operation,
        legacy_collateralized_convert_immediate_conversion_operation,
        legacy_escrow_approved_operation,
        legacy_escrow_rejected_operation
      > legacy_operation;

  struct legacy_operation_conversion_visitor
  {
    legacy_operation_conversion_visitor( legacy_operation& legacy_op ) : l_op( legacy_op ) {}

    typedef bool result_type;

    legacy_operation& l_op;

    bool operator()( const account_update_operation& op )const                 { l_op = op; return true; }
    bool operator()( const account_update2_operation& op )const                { l_op = op; return true; }
    bool operator()( const comment_operation& op )const                        { l_op = op; return true; }
    bool operator()( const create_claimed_account_operation& op )const         { l_op = op; return true; }
    bool operator()( const delete_comment_operation& op )const                 { l_op = op; return true; }
    bool operator()( const vote_operation& op )const                           { l_op = op; return true; }
    bool operator()( const escrow_approve_operation& op )const                 { l_op = op; return true; }
    bool operator()( const escrow_dispute_operation& op )const                 { l_op = op; return true; }
    bool operator()( const set_withdraw_vesting_route_operation& op )const     { l_op = op; return true; }
    bool operator()( const witness_set_properties_operation& op )const         { l_op = op; return true; }
    bool operator()( const account_witness_vote_operation& op )const           { l_op = op; return true; }
    bool operator()( const account_witness_proxy_operation& op )const          { l_op = op; return true; }
    bool operator()( const custom_operation& op )const                         { l_op = op; return true; }
    bool operator()( const custom_json_operation& op )const                    { l_op = op; return true; }
    bool operator()( const custom_binary_operation& op )const                  { l_op = op; return true; }
    bool operator()( const limit_order_cancel_operation& op )const             { l_op = op; return true; }
    bool operator()( const pow_operation& op )const                            { l_op = op; return true; }
    bool operator()( const report_over_production_operation& op )const         { l_op = op; return true; }
    bool operator()( const request_account_recovery_operation& op )const       { l_op = op; return true; }
    bool operator()( const recover_account_operation& op )const                { l_op = op; return true; }
    bool operator()( const reset_account_operation& op )const                  { l_op = op; return true; }
    bool operator()( const set_reset_account_operation& op )const              { l_op = op; return true; }
    bool operator()( const change_recovery_account_operation& op )const        { l_op = op; return true; }
    bool operator()( const cancel_transfer_from_savings_operation& op )const   { l_op = op; return true; }
    bool operator()( const decline_voting_rights_operation& op )const          { l_op = op; return true; }
    bool operator()( const shutdown_witness_operation& op )const               { l_op = op; return true; }
    bool operator()( const hardfork_operation& op )const                       { l_op = op; return true; }
    bool operator()( const comment_payout_update_operation& op )const          { l_op = op; return true; }
    bool operator()( const update_proposal_votes_operation& op )const          { l_op = op; return true; }
    bool operator()( const remove_proposal_operation& op )const                { l_op = op; return true; }
    bool operator()( const clear_null_account_balance_operation& op )const     { l_op = op; return true; }
    bool operator()( const consolidate_treasury_balance_operation& op )const   { l_op = op; return true; }
    bool operator()( const delayed_voting_operation& op )const                 { l_op = op; return true; }
    bool operator()( const expired_account_notification_operation& op )const   { l_op = op; return true; }
    bool operator()( const changed_recovery_account_operation& op )const       { l_op = op; return true; }
    bool operator()( const system_warning_operation& op )const                 { l_op = op; return true; }
    bool operator()( const ineffective_delete_comment_operation& op )const     { l_op = op; return true; }
    bool operator()( const fill_recurrent_transfer_operation& op )const        { l_op = op; return true; }
    bool operator()( const failed_recurrent_transfer_operation& op )const      { l_op = op; return true; }
    bool operator()( const producer_missed_operation& op )const                { l_op = op; return true; }

    bool operator()( const transfer_operation& op )const
    {
      l_op = legacy_transfer_operation( op );
      return true;
    }

    bool operator()( const transfer_to_vesting_operation& op )const
    {
      l_op = legacy_transfer_to_vesting_operation( op );
      return true;
    }

    bool operator()( const transfer_to_vesting_completed_operation& op )const
    {
      l_op = legacy_transfer_to_vesting_completed_operation( op );
      return true;
    }

    bool operator()( const withdraw_vesting_operation& op )const
    {
      l_op = legacy_withdraw_vesting_operation( op );
      return true;
    }

    bool operator()( const limit_order_create_operation& op )const
    {
      l_op = legacy_limit_order_create_operation( op );
      return true;
    }

    bool operator()( const feed_publish_operation& op )const
    {
      l_op = legacy_feed_publish_operation( op );
      return true;
    }

    bool operator()( const convert_operation& op )const
    {
      l_op = legacy_convert_operation( op );
      return true;
    }

    bool operator()( const collateralized_convert_operation& op )const
    {
      l_op = legacy_collateralized_convert_operation( op );
      return true;
    }

    bool operator()( const account_create_operation& op )const
    {
      l_op = legacy_account_create_operation( op );
      return true;
    }

    bool operator()( const account_created_operation& op )const
    {
      l_op = legacy_account_created_operation( op );
      return true;
    }

    bool operator()( const witness_update_operation& op )const
    {
      l_op = legacy_witness_update_operation( op );
      return true;
    }

    bool operator()( const comment_options_operation& op )const
    {
      l_op = legacy_comment_options_operation( op );
      return true;
    }

    bool operator()( const limit_order_create2_operation& op )const
    {
      l_op = legacy_limit_order_create2_operation( op );
      return true;
    }

    bool operator()( const escrow_transfer_operation& op )const
    {
      l_op = legacy_escrow_transfer_operation( op );
      return true;
    }

    bool operator()( const escrow_release_operation& op )const
    {
      l_op = legacy_escrow_release_operation( op );
      return true;
    }

    bool operator()( const pow2_operation& op )const
    {
      l_op = legacy_pow2_operation( op );
      return true;
    }

    bool operator()( const transfer_to_savings_operation& op )const
    {
      l_op = legacy_transfer_to_savings_operation( op );
      return true;
    }

    bool operator()( const transfer_from_savings_operation& op )const
    {
      l_op = legacy_transfer_from_savings_operation( op );
      return true;
    }

    bool operator()( const claim_reward_balance_operation& op )const
    {
      l_op = legacy_claim_reward_balance_operation( op );
      return true;
    }

    bool operator()( const delegate_vesting_shares_operation& op )const
    {
      l_op = legacy_delegate_vesting_shares_operation( op );
      return true;
    }

    bool operator()( const account_create_with_delegation_operation& op )const
    {
      l_op = legacy_account_create_with_delegation_operation( op );
      return true;
    }

    bool operator()( const fill_convert_request_operation& op )const
    {
      l_op = legacy_fill_convert_request_operation( op );
      return true;
    }

    bool operator()( const fill_collateralized_convert_request_operation& op )const
    {
      l_op = legacy_fill_collateralized_convert_request_operation( op );
      return true;
    }

    bool operator()( const author_reward_operation& op )const
    {
      l_op = legacy_author_reward_operation( op );
      return true;
    }

    bool operator()( const curation_reward_operation& op )const
    {
      l_op = legacy_curation_reward_operation( op );
      return true;
    }

    bool operator()( const comment_reward_operation& op )const
    {
      l_op = legacy_comment_reward_operation( op );
      return true;
    }

    bool operator()( const liquidity_reward_operation& op )const
    {
      l_op = legacy_liquidity_reward_operation( op );
      return true;
    }

    bool operator()( const interest_operation& op )const
    {
      l_op = legacy_interest_operation( op );
      return true;
    }

    bool operator()( const fill_vesting_withdraw_operation& op )const
    {
      l_op = legacy_fill_vesting_withdraw_operation( op );
      return true;
    }

    bool operator()( const fill_order_operation& op )const
    {
      l_op = legacy_fill_order_operation( op );
      return true;
    }

    bool operator()( const limit_order_cancelled_operation& op )const
    {
      l_op = legacy_limit_order_cancelled_operation( op );
      return true;
    }

    bool operator()( const fill_transfer_from_savings_operation& op )const
    {
      l_op = legacy_fill_transfer_from_savings_operation( op );
      return true;
    }

    bool operator()( const return_vesting_delegation_operation& op )const
    {
      l_op = legacy_return_vesting_delegation_operation( op );
      return true;
    }

    bool operator()( const comment_benefactor_reward_operation& op )const
    {
      l_op = legacy_comment_benefactor_reward_operation( op );
      return true;
    }

    bool operator()( const producer_reward_operation& op )const
    {
      l_op = legacy_producer_reward_operation( op );
      return true;
    }

    bool operator()( const claim_account_operation& op )const
    {
      l_op = legacy_claim_account_operation( op );
      return true;
    }

    bool operator()( const vesting_shares_split_operation& op )const
    {
      l_op = legacy_vesting_shares_split_operation( op );
      return true;
    }

    bool operator()( const pow_reward_operation& op )const
    {
      l_op = legacy_pow_reward_operation( op );
      return true;
    }

    bool operator()( const create_proposal_operation& op )const
    {
      l_op = legacy_create_proposal_operation( op );
      return true;
    }

    bool operator()( const update_proposal_operation& op )const
    {
      l_op = legacy_update_proposal_operation( op );
      return true;
    }

    bool operator()( const proposal_pay_operation& op )const
    {
      l_op = legacy_proposal_pay_operation( op );
      return true;
    }

    bool operator()( const dhf_funding_operation& op )const
    {
      l_op = legacy_dhf_funding_operation( op );
      return true;
    }

    bool operator()( const hardfork_hive_operation& op )const
    {
      l_op = legacy_hardfork_hive_operation( op );
      return true;
    }

    bool operator()( const hardfork_hive_restore_operation& op )const
    {
      l_op = legacy_hardfork_hive_restore_operation( op );
      return true;
    }

    bool operator()( const effective_comment_vote_operation& op )const
    {
      l_op = legacy_effective_comment_vote_operation( op );
      return true;
    }

    bool operator()( const recurrent_transfer_operation& op )const
    {
      l_op = legacy_recurrent_transfer_operation( op );
      return true;
    }

    bool operator()( const dhf_conversion_operation& op )const
    {
      l_op = legacy_dhf_conversion_operation( op );
      return true;
    }

    bool operator()( const proposal_fee_operation& op )const
    {
      l_op = legacy_proposal_fee_operation( op );
      return true;
    }

    bool operator()( const collateralized_convert_immediate_conversion_operation& op )const
    {
      l_op = legacy_collateralized_convert_immediate_conversion_operation( op );
      return true;
    }

    bool operator()( const escrow_approved_operation& op )const
    {
      l_op = legacy_escrow_approved_operation( op );
      return true;
    }

    bool operator()( const escrow_rejected_operation& op )const
    {
      l_op = legacy_escrow_rejected_operation( op );
      return true;
    }

#ifdef HIVE_ENABLE_SMT
    // Should only be SMT ops
    template< typename T >
    bool operator()( const T& )const { return false; }
#endif
};

struct convert_from_legacy_operation_visitor
{
  convert_from_legacy_operation_visitor() {}

  typedef operation result_type;

  operation operator()( const legacy_transfer_operation& op )const
  {
    return operation( transfer_operation( op ) );
  }

  operation operator()( const legacy_transfer_to_vesting_operation& op )const
  {
    return operation( transfer_to_vesting_operation( op ) );
  }

  operation operator()( const legacy_transfer_to_vesting_completed_operation& op )const
  {
    return operation( transfer_to_vesting_completed_operation( op ) );
  }

  operation operator()( const legacy_withdraw_vesting_operation& op )const
  {
    return operation( withdraw_vesting_operation( op ) );
  }

  operation operator()( const legacy_limit_order_create_operation& op )const
  {
    return operation( limit_order_create_operation( op ) );
  }

  operation operator()( const legacy_feed_publish_operation& op )const
  {
    return operation( feed_publish_operation( op ) );
  }

  operation operator()( const legacy_convert_operation& op )const
  {
    return operation( convert_operation( op ) );
  }

  operation operator()( const legacy_collateralized_convert_operation& op )const
  {
    return operation( collateralized_convert_operation( op ) );
  }

  operation operator()( const legacy_account_create_operation& op )const
  {
    return operation( account_create_operation( op ) );
  }

  operation operator()( const legacy_account_created_operation& op )const
  {
    return operation( account_created_operation( op ) );
  }

  operation operator()( const legacy_witness_update_operation& op )const
  {
    return operation( witness_update_operation( op ) );
  }

  operation operator()( const legacy_comment_options_operation& op )const
  {
    return operation( comment_options_operation( op ) );
  }

  operation operator()( const legacy_limit_order_create2_operation& op )const
  {
    return operation( limit_order_create2_operation( op ) );
  }

  operation operator()( const legacy_escrow_transfer_operation& op )const
  {
    return operation( escrow_transfer_operation( op ) );
  }

  operation operator()( const legacy_escrow_release_operation& op )const
  {
    return operation( escrow_release_operation( op ) );
  }

  operation operator()( const legacy_pow2_operation& op )const
  {
    return operation( pow2_operation( op ) );
  }

  operation operator()( const legacy_transfer_to_savings_operation& op )const
  {
    return operation( transfer_to_savings_operation( op ) );
  }

  operation operator()( const legacy_transfer_from_savings_operation& op )const
  {
    return operation( transfer_from_savings_operation( op ) );
  }

  operation operator()( const legacy_claim_reward_balance_operation& op )const
  {
    return operation( claim_reward_balance_operation( op ) );
  }

  operation operator()( const legacy_delegate_vesting_shares_operation& op )const
  {
    return operation( delegate_vesting_shares_operation( op ) );
  }

  operation operator()( const legacy_account_create_with_delegation_operation& op )const
  {
    return operation( account_create_with_delegation_operation( op ) );
  }

  operation operator()( const legacy_fill_convert_request_operation& op )const
  {
    return operation( fill_convert_request_operation( op ) );
  }

  operation operator()( const legacy_fill_collateralized_convert_request_operation& op )const
  {
    return operation( fill_collateralized_convert_request_operation( op ) );
  }

  operation operator()( const legacy_author_reward_operation& op )const
  {
    return operation( author_reward_operation( op ) );
  }

  operation operator()( const legacy_curation_reward_operation& op )const
  {
    return operation( curation_reward_operation( op ) );
  }

  operation operator()( const legacy_comment_reward_operation& op )const
  {
    return operation( comment_reward_operation( op ) );
  }

  operation operator()( const legacy_liquidity_reward_operation& op )const
  {
    return operation( liquidity_reward_operation( op ) );
  }

  operation operator()( const legacy_interest_operation& op )const
  {
    return operation( interest_operation( op ) );
  }

  operation operator()( const legacy_fill_vesting_withdraw_operation& op )const
  {
    return operation( fill_vesting_withdraw_operation( op ) );
  }

  operation operator()( const legacy_fill_order_operation& op )const
  {
    return operation( fill_order_operation( op ) );
  }

  operation operator()( const legacy_limit_order_cancelled_operation& op )const
  {
    return operation( limit_order_cancelled_operation( op ) );
  }

  operation operator()( const legacy_fill_transfer_from_savings_operation& op )const
  {
    return operation( fill_transfer_from_savings_operation( op ) );
  }

  operation operator()( const legacy_return_vesting_delegation_operation& op )const
  {
    return operation( return_vesting_delegation_operation( op ) );
  }

  operation operator()( const legacy_comment_benefactor_reward_operation& op )const
  {
    return operation( comment_benefactor_reward_operation( op ) );
  }

  operation operator()( const legacy_producer_reward_operation& op )const
  {
    return operation( producer_reward_operation( op ) );
  }

  operation operator()( const legacy_claim_account_operation& op )const
  {
    return operation( claim_account_operation( op ) );
  }

  operation operator()( const legacy_vesting_shares_split_operation& op )const
  {
    return operation( vesting_shares_split_operation( op ) );
  }

  operation operator()( const legacy_pow_reward_operation& op )const
  {
    return operation( pow_reward_operation( op ) );
  }

  operation operator()( const legacy_create_proposal_operation& op )const
  {
    return operation( create_proposal_operation( op ) );
  }

  operation operator()( const legacy_update_proposal_operation& op )const
  {
    return operation( update_proposal_operation( op ) );
  }

  operation operator()( const legacy_proposal_pay_operation& op )const
  {
    return operation( proposal_pay_operation( op ) );
  }

  operation operator()( const legacy_dhf_funding_operation& op )const
  {
    return operation( dhf_funding_operation( op ) );
  }

  operation operator()( const legacy_hardfork_hive_operation& op )const
  {
    return operation( hardfork_hive_operation( op ) );
  }

  operation operator()( const legacy_hardfork_hive_restore_operation& op )const
  {
    return operation( hardfork_hive_restore_operation( op ) );
  }

  operation operator()( const legacy_effective_comment_vote_operation& op )const
  {
    return operation( effective_comment_vote_operation( op ) );
  }

  operation operator()( const legacy_recurrent_transfer_operation& op )const
  {
    return operation( recurrent_transfer_operation( op ) );
  }

  operation operator()( const legacy_dhf_conversion_operation& op )const
  {
    return operation( dhf_conversion_operation( op ) );
  }

  operation operator()( const legacy_proposal_fee_operation& op )const
  {
    return operation( proposal_fee_operation( op ) );
  }

  operation operator()( const legacy_collateralized_convert_immediate_conversion_operation& op )const
  {
    return operation( collateralized_convert_immediate_conversion_operation( op ) );
  }

  operation operator()( const legacy_escrow_approved_operation& op )const
  {
    return operation( escrow_approved_operation( op ) );
  }

  operation operator()( const legacy_escrow_rejected_operation& op )const
  {
    return operation( escrow_rejected_operation( op ) );
  }

  template< typename T >
  operation operator()( const T& t )const
  {
    return operation( t );
  }
};

} } } // hive::plugins::condenser_api

namespace fc {

void to_variant( const hive::plugins::condenser_api::legacy_operation&, fc::variant& );
void from_variant( const fc::variant&, hive::plugins::condenser_api::legacy_operation& );

struct from_old_static_variant
{
  variant& var;
  from_old_static_variant( variant& dv ):var(dv){}

  typedef void result_type;
  template<typename T> void operator()( const T& v )const
  {
    to_variant( v, var );
  }
};

struct to_old_static_variant
{
  const variant& var;
  to_old_static_variant( const variant& dv ):var(dv){}

  typedef void result_type;
  template<typename T> void operator()( T& v )const
  {
    from_variant( var, v );
  }
};

template< typename T >
void old_sv_to_variant( const T& sv, fc::variant& v )
{
  variant tmp;
  variants vars(2);
  vars[0] = sv.which();
  sv.visit( from_old_static_variant(vars[1]) );
  v = std::move(vars);
}

template< typename T >
void old_sv_from_variant( const fc::variant& v, T& sv )
{
  auto ar = v.get_array();
  if( ar.size() < 2 ) return;
  sv.set_which( static_cast< int64_t >( ar[0].as_uint64() ) );
  sv.visit( to_old_static_variant(ar[1]) );
}

template< typename T >
void new_sv_from_variant( const fc::variant& v, T& sv )
{
  FC_ASSERT( v.is_object(), "Input data have to treated as object." );
  auto v_object = v.get_object();

  FC_ASSERT( v_object.contains( "type" ), "Type field doesn't exist." );
  FC_ASSERT( v_object.contains( "value" ), "Value field doesn't exist." );

  auto ar = v.get_object();
  sv.set_which( static_cast< int64_t >( ar["type"].as_uint64() ) );
  sv.visit( to_old_static_variant(ar["value"]) );
}

// allows detection of pre-rebranding calls and special error reaction
// note: that and related code will be removed some time after HF24
//       since all nodes and libraries should be updated by then
struct obsolete_call_detector
{
  static bool enable_obsolete_call_detection;

  static void report_error()
  {
    FC_ASSERT( false, "Obsolete form of transaction detected, update your wallet." );
  }
};

}

#define FC_REFLECT_ALIAS_HANDLER( r, name, elem )                                  \
  || ( ( strcmp( name, std::make_pair elem .first ) == 0 ) &&                      \
       ( vo.find( std::make_pair elem .second ) != vo.end() ) )

#define FC_REFLECT_ALIASED_NAMES( TYPE, MEMBERS, ALIASES )                         \
FC_REFLECT( TYPE, MEMBERS )                                                        \
namespace fc {                                                                     \
                                                                                   \
template<>                                                                         \
class from_variant_visitor<TYPE> : obsolete_call_detector                          \
{                                                                                  \
public:                                                                            \
from_variant_visitor( const variant_object& _vo, TYPE& v ) :vo( _vo ), val( v ) {} \
                                                                                   \
template<typename Member, class Class, Member( Class::* member )>                  \
void operator()( const char* name )const                                           \
{                                                                                  \
  auto itr = vo.find( name );                                                      \
  if( itr != vo.end() )                                                            \
    from_variant( itr->value(), val.*member );                                     \
  else if( enable_obsolete_call_detection && ( false                               \
    BOOST_PP_SEQ_FOR_EACH( FC_REFLECT_ALIAS_HANDLER, name, ALIASES )               \
  ) )                                                                              \
    report_error();                                                                \
}                                                                                  \
                                                                                   \
const variant_object& vo;                                                          \
TYPE& val;                                                                         \
};                                                                                 \
                                                                                   \
}

FC_REFLECT( hive::plugins::condenser_api::api_chain_properties,
        (account_creation_fee)(maximum_block_size)(hbd_interest_rate)(account_subsidy_budget)(account_subsidy_decay)
        )

FC_REFLECT( hive::plugins::condenser_api::legacy_price, (base)(quote) )
FC_REFLECT( hive::plugins::condenser_api::legacy_transfer_to_savings_operation, (from)(to)(amount)(memo) )
FC_REFLECT( hive::plugins::condenser_api::legacy_transfer_from_savings_operation, (from)(request_id)(to)(amount)(memo) )
FC_REFLECT( hive::plugins::condenser_api::legacy_convert_operation, (owner)(requestid)(amount) )
FC_REFLECT( hive::plugins::condenser_api::legacy_collateralized_convert_operation, (owner)(requestid)(amount) )
FC_REFLECT( hive::plugins::condenser_api::legacy_feed_publish_operation, (publisher)(exchange_rate) )

FC_REFLECT( hive::plugins::condenser_api::legacy_account_create_operation,
        (fee)
        (creator)
        (new_account_name)
        (owner)
        (active)
        (posting)
        (memo_key)
        (json_metadata) )

FC_REFLECT( hive::plugins::condenser_api::legacy_account_created_operation,
            (new_account_name)
            (creator)
            (initial_vesting_shares)
            (initial_delegation))

FC_REFLECT( hive::plugins::condenser_api::legacy_account_create_with_delegation_operation,
        (fee)
        (delegation)
        (creator)
        (new_account_name)
        (owner)
        (active)
        (posting)
        (memo_key)
        (json_metadata)
        (extensions) )

FC_REFLECT( hive::plugins::condenser_api::legacy_transfer_operation, (from)(to)(amount)(memo) )
FC_REFLECT( hive::plugins::condenser_api::legacy_transfer_to_vesting_operation, (from)(to)(amount) )
FC_REFLECT( hive::plugins::condenser_api::legacy_transfer_to_vesting_completed_operation, (from_account)(to_account)(hive_vested)(vesting_shares_received) )
FC_REFLECT( hive::plugins::condenser_api::legacy_withdraw_vesting_operation, (account)(vesting_shares) )
FC_REFLECT( hive::plugins::condenser_api::legacy_witness_update_operation, (owner)(url)(block_signing_key)(props)(fee) )
FC_REFLECT( hive::plugins::condenser_api::legacy_limit_order_create_operation, (owner)(orderid)(amount_to_sell)(min_to_receive)(fill_or_kill)(expiration) )
FC_REFLECT( hive::plugins::condenser_api::legacy_limit_order_create2_operation, (owner)(orderid)(amount_to_sell)(exchange_rate)(fill_or_kill)(expiration) )
FC_REFLECT_ALIASED_NAMES( hive::plugins::condenser_api::legacy_comment_options_operation,
  (author)(permlink)(max_accepted_payout)(percent_hbd)(allow_votes)(allow_curation_rewards)(extensions),
  ( ("percent_hbd", "percent_steem_dollars") )
)
FC_REFLECT_ALIASED_NAMES( hive::plugins::condenser_api::legacy_escrow_transfer_operation,
  (from)(to)(hbd_amount)(hive_amount)(escrow_id)(agent)(fee)(json_meta)(ratification_deadline)(escrow_expiration),
  ( ("hbd_amount", "sbd_amount") )( ("hive_amount", "steem_amount") )
)
FC_REFLECT_ALIASED_NAMES( hive::plugins::condenser_api::legacy_escrow_release_operation,
  (from)(to)(agent)(who)(receiver)(escrow_id)(hbd_amount)(hive_amount),
  ( ("hbd_amount", "sbd_amount") )( ("hive_amount", "steem_amount") )
)
FC_REFLECT( hive::plugins::condenser_api::legacy_pow2_operation, (work)(new_owner_key)(props) )

FC_REFLECT_ALIASED_NAMES( hive::plugins::condenser_api::legacy_claim_reward_balance_operation,
  (account)(reward_hive)(reward_hbd)(reward_vests),
  ( ("reward_hive", "reward_steem") ) ( ("reward_hbd", "reward_sbd") )
)
FC_REFLECT( hive::plugins::condenser_api::legacy_delegate_vesting_shares_operation, (delegator)(delegatee)(vesting_shares) )
FC_REFLECT( hive::plugins::condenser_api::legacy_author_reward_operation, (author)(permlink)(hbd_payout)(hive_payout)(vesting_payout)(payout_must_be_claimed) )
FC_REFLECT( hive::plugins::condenser_api::legacy_curation_reward_operation, (curator)(reward)(comment_author)(comment_permlink)(payout_must_be_claimed) )
FC_REFLECT( hive::plugins::condenser_api::legacy_comment_reward_operation, (author)(permlink)(payout) )
FC_REFLECT( hive::plugins::condenser_api::legacy_fill_convert_request_operation, (owner)(requestid)(amount_in)(amount_out) )
FC_REFLECT( hive::plugins::condenser_api::legacy_fill_collateralized_convert_request_operation, (owner)(requestid)(amount_in)(amount_out)(excess_collateral) )
FC_REFLECT( hive::plugins::condenser_api::legacy_liquidity_reward_operation, (owner)(payout) )
FC_REFLECT( hive::plugins::condenser_api::legacy_interest_operation, (owner)(interest) )
FC_REFLECT( hive::plugins::condenser_api::legacy_fill_vesting_withdraw_operation, (from_account)(to_account)(withdrawn)(deposited) )
FC_REFLECT( hive::plugins::condenser_api::legacy_fill_order_operation, (current_owner)(current_orderid)(current_pays)(open_owner)(open_orderid)(open_pays) )
FC_REFLECT( hive::plugins::condenser_api::legacy_limit_order_cancelled_operation, (seller)(orderid)(amount_back) )
FC_REFLECT( hive::plugins::condenser_api::legacy_fill_transfer_from_savings_operation, (from)(to)(amount)(request_id)(memo) )
FC_REFLECT( hive::plugins::condenser_api::legacy_return_vesting_delegation_operation, (account)(vesting_shares) )
FC_REFLECT( hive::plugins::condenser_api::legacy_comment_benefactor_reward_operation, (benefactor)(author)(permlink)(hbd_payout)(hive_payout)(vesting_payout)(payout_must_be_claimed) )
FC_REFLECT( hive::plugins::condenser_api::legacy_producer_reward_operation, (producer)(vesting_shares) )
FC_REFLECT( hive::plugins::condenser_api::legacy_claim_account_operation, (creator)(fee)(extensions) )
FC_REFLECT( hive::plugins::condenser_api::legacy_vesting_shares_split_operation, (owner)(vesting_shares_before_split)(vesting_shares_after_split) )
FC_REFLECT( hive::plugins::condenser_api::legacy_pow_reward_operation, (worker)(reward) )
FC_REFLECT( hive::plugins::condenser_api::legacy_proposal_pay_operation, (proposal_id)(receiver)(payer)(payment)(trx_id)(op_in_trx) )
FC_REFLECT( hive::plugins::condenser_api::legacy_dhf_funding_operation, (treasury)(additional_funds) )
FC_REFLECT( hive::plugins::condenser_api::legacy_create_proposal_operation, (creator)(receiver)(start_date)(end_date)(daily_pay)(subject)(permlink) )
FC_REFLECT( hive::plugins::condenser_api::legacy_update_proposal_operation, (proposal_id)(creator)(daily_pay)(subject)(permlink)(extensions) )
FC_REFLECT( hive::plugins::condenser_api::legacy_hardfork_hive_operation, (account)(treasury)(other_affected_accounts)(hbd_transferred)(hive_transferred)(vests_converted)(total_hive_from_vests) )
FC_REFLECT( hive::plugins::condenser_api::legacy_hardfork_hive_restore_operation, (account)(treasury)(hbd_transferred)(hive_transferred) )
FC_REFLECT( hive::plugins::condenser_api::legacy_effective_comment_vote_operation, (voter)(author)(permlink)(weight)(rshares)(total_vote_weight)(pending_payout) )
FC_REFLECT( hive::plugins::condenser_api::legacy_recurrent_transfer_operation, (from)(to)(amount)(memo)(recurrence)(executions) )
FC_REFLECT( hive::plugins::condenser_api::legacy_dhf_conversion_operation, (treasury)(hive_amount_in)(hbd_amount_out) )
FC_REFLECT( hive::plugins::condenser_api::legacy_proposal_fee_operation, (creator)(treasury)(proposal_id)(fee) )
FC_REFLECT( hive::plugins::condenser_api::legacy_collateralized_convert_immediate_conversion_operation, (owner)(requestid)(hbd_out) )
FC_REFLECT( hive::plugins::condenser_api::legacy_escrow_approved_operation, (from)(to)(agent)(escrow_id)(fee) )
FC_REFLECT( hive::plugins::condenser_api::legacy_escrow_rejected_operation, (from)(to)(agent)(escrow_id)(hbd_amount)(hive_amount)(fee) )

FC_REFLECT_TYPENAME( hive::plugins::condenser_api::legacy_operation )
