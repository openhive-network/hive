#pragma once
#include <hive/protocol/base.hpp>
#include <hive/protocol/block_header.hpp>
#include <hive/protocol/asset.hpp>
#include <hive/protocol/validation.hpp>
#include <hive/protocol/legacy_asset.hpp>
#include <hive/protocol/json_string.hpp>

#include <fc/crypto/equihash.hpp>

namespace hive { namespace protocol {
  void validate_auth_size( const authority& a );

  struct account_create_operation : public base_operation
  {
    asset             fee;
    account_name_type creator;
    account_name_type new_account_name;
    authority         owner;
    authority         active;
    authority         posting;
    public_key_type   memo_key;
    json_string       json_metadata;

    void validate()const;
    void get_required_active_authorities( flat_set<account_name_type>& a )const{ a.insert(creator); }
  };


  struct account_create_with_delegation_operation : public base_operation
  {
    asset             fee;
    asset             delegation;
    account_name_type creator;
    account_name_type new_account_name;
    authority         owner;
    authority         active;
    authority         posting;
    public_key_type   memo_key;
    json_string       json_metadata;

    extensions_type   extensions;

    void validate()const;
    void get_required_active_authorities( flat_set<account_name_type>& a )const{ a.insert(creator); }
  };


  struct account_update_operation : public base_operation
  {
    account_name_type             account;
    optional< authority >         owner;
    optional< authority >         active;
    optional< authority >         posting;
    public_key_type               memo_key;
    json_string                   json_metadata;

    void validate()const;

    void get_required_owner_authorities( flat_set<account_name_type>& a )const
    { if( owner ) a.insert( account ); }

    void get_required_active_authorities( flat_set<account_name_type>& a )const
    { if( !owner ) a.insert( account ); }
  };

  struct account_update2_operation : public base_operation
  {
    account_name_type             account;
    optional< authority >         owner;
    optional< authority >         active;
    optional< authority >         posting;
    optional< public_key_type >   memo_key;
    json_string                   json_metadata;
    json_string                   posting_json_metadata;

    extensions_type               extensions;

    void validate()const;

    void get_required_owner_authorities( flat_set<account_name_type>& a )const
    { if( owner ) a.insert( account ); }

    void get_required_active_authorities( flat_set<account_name_type>& a )const
    { if( !owner && ( active || posting || memo_key || json_metadata.size() > 0 ) ) a.insert( account ); }

    void get_required_posting_authorities( flat_set<account_name_type>& a )const
    { if( !( owner || active || posting || memo_key || json_metadata.size() > 0 ) ) a.insert( account ); }
  };

  struct comment_operation : public base_operation
  {
    account_name_type parent_author;
    string            parent_permlink;

    account_name_type author;
    string            permlink;

    string            title;
    string            body;
    json_string       json_metadata;

    void validate()const;
    void get_required_posting_authorities( flat_set<account_name_type>& a )const{ a.insert(author); }
  };

  struct beneficiary_route_type
  {
    beneficiary_route_type() {}
    beneficiary_route_type( const account_name_type& a, const uint16_t& w ) : account( a ), weight( w ){}

    account_name_type account;
    uint16_t          weight;

    // For use by std::sort such that the route is sorted first by name (ascending)
    bool operator < ( const beneficiary_route_type& o )const { return account < o.account; }
  };

  struct comment_payout_beneficiaries
  {
    vector< beneficiary_route_type > beneficiaries;

    void validate()const;
  };

#ifdef HIVE_ENABLE_SMT
  struct votable_asset_info_v1
  {
    votable_asset_info_v1() = default;
    votable_asset_info_v1(const share_type& max_payout, bool allow_rewards) :
      max_accepted_payout(max_payout), allow_curation_rewards(allow_rewards) {}

    share_type        max_accepted_payout    = 0;
    bool              allow_curation_rewards = false;
  };

  typedef static_variant< votable_asset_info_v1 > votable_asset_info;

  /** Allows to store all SMT tokens being allowed to use during voting process.
    *  Maps asset symbol (SMT) to the vote info.
    *  @see SMT spec for details: https://gitlab.syncad.com/hive/smt-whitepaper/blob/master/smt-manual/manual.md
    */
  struct allowed_vote_assets
  {
    /// Helper method to simplify construction of votable_asset_info.
    void add_votable_asset(const asset_symbol_type& symbol, const share_type& max_accepted_payout,
      bool allow_curation_rewards)
      {
        votable_asset_info info(votable_asset_info_v1(max_accepted_payout, allow_curation_rewards));
        votable_assets[symbol] = std::move(info);
      }

    /** Allows to check if given symbol is allowed votable asset.
      *  @param symbol - asset symbol to be check against votable feature
      *  @param max_accepted_payout - optional output parameter which allows to take `max_accepted_payout`
      *                  configured for given asset
      *  @param allow_curation_rewards - optional output parameter which allows to take `allow_curation_rewards`
      *                  specified for given votable asset.
      *  @returns true if given asset is allowed votable asset for given comment.
      */
    bool is_allowed(const asset_symbol_type& symbol, share_type* max_accepted_payout = nullptr,
      bool* allow_curation_rewards = nullptr) const
      {
        auto foundI = votable_assets.find(symbol);
        if(foundI == votable_assets.end())
        {
          if(max_accepted_payout != nullptr)
            *max_accepted_payout = 0;
          if(allow_curation_rewards != nullptr)
            *allow_curation_rewards = false;
          return false;
        }

        if(max_accepted_payout != nullptr)
          *max_accepted_payout = foundI->second.get<votable_asset_info_v1>().max_accepted_payout;
        if(allow_curation_rewards != nullptr)
          *allow_curation_rewards = foundI->second.get<votable_asset_info_v1>().allow_curation_rewards;

        return true;
      }

    /** Part of `comment_option_operation` validation process, to be called when allowed_vote_assets object
      *  has been added as comment option extension.
      *  @throws fc::assert_exception on failure.
      */
    void validate() const
    {
      FC_ASSERT(votable_assets.size() <= SMT_MAX_VOTABLE_ASSETS, "Too much votable assets specified");
      FC_ASSERT(is_allowed(HIVE_SYMBOL) == false,
        "HIVE can not be explicitly specified as one of allowed_vote_assets");
    }

    flat_map< asset_symbol_type, votable_asset_info > votable_assets;
  };
#endif /// HIVE_ENABLE_SMT

  typedef static_variant<
        comment_payout_beneficiaries
#ifdef HIVE_ENABLE_SMT
        ,allowed_vote_assets
#endif /// HIVE_ENABLE_SMT
        > comment_options_extension;

  typedef flat_set< comment_options_extension > comment_options_extensions_type;

  /**
    *  Authors of posts may not want all of the benefits that come from creating a post. This
    *  operation allows authors to update properties associated with their post.
    *
    *  The max_accepted_payout may be decreased, but never increased.
    *  The percent_hbd may be decreased, but never increased
    */
  struct comment_options_operation : public base_operation
  {
    account_name_type author;
    string            permlink;

    asset             max_accepted_payout    = asset( 1000000000, HBD_SYMBOL ); /// HBD value of the maximum payout this post will receive
    uint16_t          percent_hbd            = HIVE_100_PERCENT; /// the percent of HBD to key, unkept amounts will be received in form of VESTS
    bool              allow_votes            = true; /// allows a post to receive votes
    bool              allow_curation_rewards = true; /// allows voters to recieve curation rewards. Rewards return to reward fund.
    comment_options_extensions_type extensions;

    void validate()const;
    void get_required_posting_authorities( flat_set<account_name_type>& a )const{ a.insert(author); }
  };


  struct claim_account_operation : public base_operation
  {
    account_name_type creator;
    asset             fee;
    extensions_type   extensions;

    void get_required_active_authorities( flat_set< account_name_type >& a ) const { a.insert( creator ); }
    void validate() const;
  };


  struct create_claimed_account_operation : public base_operation
  {
    account_name_type creator;
    account_name_type new_account_name;
    authority         owner;
    authority         active;
    authority         posting;
    public_key_type   memo_key;
    json_string       json_metadata;
    extensions_type   extensions;

    void get_required_active_authorities( flat_set< account_name_type >& a ) const { a.insert( creator ); }
    void validate() const;
  };


  struct delete_comment_operation : public base_operation
  {
    account_name_type author;
    string            permlink;

    void validate()const;
    void get_required_posting_authorities( flat_set<account_name_type>& a )const{ a.insert(author); }
  };


  struct vote_operation : public base_operation
  {
    account_name_type    voter;
    account_name_type    author;
    string               permlink;
    int16_t              weight = 0;

    void validate()const;
    void get_required_posting_authorities( flat_set<account_name_type>& a )const{ a.insert(voter); }
  };


  /**
    * @ingroup operations
    *
    * @brief Transfers any liquid asset (nonvesting) from one account to another.
    */
  struct transfer_operation : public base_operation
  {
    account_name_type from;
    /// Account to transfer asset to
    account_name_type to;
    /// The amount of asset to transfer from @ref from to @ref to
    asset             amount;

    /// The memo is plain-text, any encryption on the memo is up to a higher level protocol.
    string            memo;

    void              validate()const;
    void get_required_active_authorities( flat_set<account_name_type>& a )const{ a.insert(from); }
  };


  /**
    *  The purpose of this operation is to enable someone to send money contingently to
    *  another individual. The funds leave the *from* account and go into a temporary balance
    *  where they are held until *from* releases it to *to* or *to* refunds it to *from*.
    *
    *  In the event of a dispute the *agent* can divide the funds between the to/from account.
    *  Disputes can be raised any time before or on the dispute deadline time, after the escrow
    *  has been approved by all parties.
    *
    *  This operation only creates a proposed escrow transfer. Both the *agent* and *to* must
    *  agree to the terms of the arrangement by approving the escrow.
    *
    *  The escrow agent is paid the fee on approval of all parties. It is up to the escrow agent
    *  to determine the fee.
    *
    *  Escrow transactions are uniquely identified by 'from' and 'escrow_id', the 'escrow_id' is defined
    *  by the sender.
    */
  struct escrow_transfer_operation : public base_operation
  {
    account_name_type from;
    account_name_type to;
    account_name_type agent;
    uint32_t          escrow_id = 30;

    asset             hbd_amount = asset( 0, HBD_SYMBOL );
    asset             hive_amount = asset( 0, HIVE_SYMBOL );
    asset             fee;

    time_point_sec    ratification_deadline;
    time_point_sec    escrow_expiration;

    json_string       json_meta;

    void validate()const;
    void get_required_active_authorities( flat_set<account_name_type>& a )const{ a.insert(from); }
  };


  /**
    *  The agent and to accounts must approve an escrow transaction for it to be valid on
    *  the blockchain. Once a part approves the escrow, the cannot revoke their approval.
    *  Subsequent escrow approve operations, regardless of the approval, will be rejected.
    */
  struct escrow_approve_operation : public base_operation
  {
    account_name_type from;
    account_name_type to;
    account_name_type agent;
    account_name_type who; // Either to or agent

    uint32_t          escrow_id = 30;
    bool              approve = true;

    void validate()const;
    void get_required_active_authorities( flat_set<account_name_type>& a )const{ a.insert(who); }
  };


  /**
    *  If either the sender or receiver of an escrow payment has an issue, they can
    *  raise it for dispute. Once a payment is in dispute, the agent has authority over
    *  who gets what.
    */
  struct escrow_dispute_operation : public base_operation
  {
    account_name_type from;
    account_name_type to;
    account_name_type agent;
    account_name_type who;

    uint32_t          escrow_id = 30;

    void validate()const;
    void get_required_active_authorities( flat_set<account_name_type>& a )const{ a.insert(who); }
  };


  /**
    *  This operation can be used by anyone associated with the escrow transfer to
    *  release funds if they have permission.
    *
    *  The permission scheme is as follows:
    *  If there is no dispute and escrow has not expired, either party can release funds to the other.
    *  If escrow expires and there is no dispute, either party can release funds to either party.
    *  If there is a dispute regardless of expiration, the agent can release funds to either party
    *     following whichever agreement was in place between the parties.
    */
  struct escrow_release_operation : public base_operation
  {
    account_name_type from;
    account_name_type to; ///< the original 'to'
    account_name_type agent;
    account_name_type who; ///< the account that is attempting to release the funds, determines valid 'receiver'
    account_name_type receiver; ///< the account that should receive funds (might be from, might be to)

    uint32_t          escrow_id = 30;
    asset             hbd_amount = asset( 0, HBD_SYMBOL ); ///< the amount of HBD to release
    asset             hive_amount = asset( 0, HIVE_SYMBOL ); ///< the amount of HIVE to release

    void validate()const;
    void get_required_active_authorities( flat_set<account_name_type>& a )const{ a.insert(who); }
  };


  /**
    *  This operation converts liquid token (HIVE or liquid SMT) into VFS (Vesting Fund Shares,
    *  VESTS or vesting SMT) at the current exchange rate. With this operation it is possible to
    *  give another account vesting shares so that faucets can pre-fund new accounts with vesting shares.
    */
  struct transfer_to_vesting_operation : public base_operation
  {
    account_name_type from;
    account_name_type to;      ///< if null, then same as from
    asset             amount;  ///< must be HIVE or liquid variant of SMT

    void validate()const;
    void get_required_active_authorities( flat_set<account_name_type>& a )const{ a.insert(from); }
  };


  /**
    * At any given point in time an account can be withdrawing from their
    * vesting shares. A user may change the number of shares they wish to
    * cash out at any time between 0 and their total vesting stake.
    *
    * After applying this operation, vesting_shares will be withdrawn
    * at a rate of vesting_shares/104 per week for two years starting
    * one week after this operation is included in the blockchain.
    *
    * This operation is not valid if the user has no vesting shares.
    */
  struct withdraw_vesting_operation : public base_operation
  {
    account_name_type account;
    asset             vesting_shares;

    void validate()const;
    void get_required_active_authorities( flat_set<account_name_type>& a )const{ a.insert(account); }
  };


  /**
    * Allows an account to setup a vesting withdraw but with the additional
    * request for the funds to be transferred directly to another account's
    * balance rather than the withdrawing account. In addition, those funds
    * can be immediately vested again, circumventing the conversion from
    * VESTS to HIVE and back, guaranteeing they maintain their value.
    */
  struct set_withdraw_vesting_route_operation : public base_operation
  {
    account_name_type from_account;
    account_name_type to_account;
    uint16_t          percent = 0;
    bool              auto_vest = false;

    void validate()const;
    void get_required_active_authorities( flat_set<account_name_type>& a )const { a.insert( from_account ); }
  };


  /**
    * Witnesses must vote on how to set certain chain properties to ensure a smooth
    * and well functioning network.  Any time @owner is in the active set of witnesses these
    * properties will be used to control the blockchain configuration.
    */
  struct legacy_chain_properties
  {
    /**
      *  This fee, paid in HIVE, is converted into VESTS for the new account. Accounts
      *  without vesting shares cannot earn usage rations and therefore are powerless. This minimum
      *  fee requires all accounts to have some kind of commitment to the network that includes the
      *  ability to vote and make transactions.
      */
    legacy_hive_asset account_creation_fee = legacy_hive_asset::from_amount( HIVE_MIN_ACCOUNT_CREATION_FEE );

    /**
      *  This witnesses vote for the maximum_block_size which is used by the network
      *  to tune rate limiting and capacity
      */
    uint32_t          maximum_block_size = HIVE_MIN_BLOCK_SIZE_LIMIT * 2;
    uint16_t          hbd_interest_rate  = HIVE_DEFAULT_HBD_INTEREST_RATE;

    template< bool force_canon >
    void validate()const
    {
      if( force_canon )
      {
        FC_ASSERT( account_creation_fee.symbol.is_canon() );
      }
      FC_ASSERT( account_creation_fee.amount >= HIVE_MIN_ACCOUNT_CREATION_FEE);
      FC_ASSERT( maximum_block_size >= HIVE_MIN_BLOCK_SIZE_LIMIT);
      FC_ASSERT( hbd_interest_rate >= 0 );
      FC_ASSERT( hbd_interest_rate <= HIVE_100_PERCENT );
    }
  };


  /**
    *  Users who wish to become a witness must pay a fee acceptable to
    *  the current witnesses to apply for the position and allow voting
    *  to begin.
    *
    *  If the owner isn't a witness they will become a witness.  Witnesses
    *  are charged a fee equal to 1 weeks worth of witness pay which in
    *  turn is derived from the current share supply.  The fee is
    *  only applied if the owner is not already a witness.
    *
    *  If the block_signing_key is null then the witness is removed from
    *  contention.  The network will pick the top 21 witnesses for
    *  producing blocks.
    */
  struct witness_update_operation : public base_operation
  {
    account_name_type owner;
    string            url;
    public_key_type   block_signing_key;
    legacy_chain_properties  props;
    asset             fee; ///< the fee paid to register a new witness, should be 10x current block production pay

    void validate()const;
    void get_required_active_authorities( flat_set<account_name_type>& a )const{ a.insert(owner); }
  };

  struct witness_set_properties_operation : public base_operation
  {
    account_name_type                   owner;
    flat_map< string, vector< char > >  props;
    extensions_type                     extensions;

    void validate()const;
    void get_required_authorities( vector< authority >& a )const
    {
      auto key_itr = props.find( "key" );

      if( key_itr != props.end() )
      {
        public_key_type signing_key;
        fc::raw::unpack_from_vector( key_itr->second, signing_key );
        a.push_back( authority( 1, signing_key, 1 ) );
      }
      else
        a.push_back( authority( 1, HIVE_NULL_ACCOUNT, 1 ) ); // The null account auth is impossible to satisfy
    }
  };

  /**
    * All accounts with a VFS can vote for or against any witness.
    *
    * If a proxy is specified then all existing votes are removed.
    */
  struct account_witness_vote_operation : public base_operation
  {
    account_name_type account;
    account_name_type witness;
    bool              approve = true;

    void validate() const;
    void get_required_active_authorities( flat_set<account_name_type>& a )const{ a.insert(account); }
  };


  struct account_witness_proxy_operation : public base_operation
  {
    account_name_type account;
    account_name_type proxy = HIVE_PROXY_TO_SELF_ACCOUNT;

    void validate()const;
    void get_required_active_authorities( flat_set<account_name_type>& a )const{ a.insert(account); }

    bool is_clearing_proxy() const { return proxy == HIVE_PROXY_TO_SELF_ACCOUNT; }
  };


  /**
    * @brief provides a generic way to add higher level protocols on top of witness consensus
    * @ingroup operations
    *
    * There is no validation for this operation other than that required auths are valid
    */
  struct custom_operation : public base_operation
  {
    flat_set< account_name_type > required_auths;
    uint16_t                      id = 0;
    vector< char >                data;

    void validate()const;
    void get_required_active_authorities( flat_set<account_name_type>& a )const{ for( const auto& i : required_auths ) a.insert(i); }
  };

  /** serves the same purpose as custom_operation but also supports required posting authorities. Unlike custom_operation,
    * this operation is designed to be human readable/developer friendly.
    **/
  struct custom_json_operation : public base_operation
  {
    flat_set< account_name_type > required_auths;
    flat_set< account_name_type > required_posting_auths;
    custom_id_type                id; ///< must be less than 32 characters long
    json_string                   json; ///< must be proper utf8 / JSON string.

    void validate()const;
    void get_required_active_authorities( flat_set<account_name_type>& a )const{ for( const auto& i : required_auths ) a.insert(i); }
    void get_required_posting_authorities( flat_set<account_name_type>& a )const{ for( const auto& i : required_posting_auths ) a.insert(i); }
  };


  struct custom_binary_operation : public base_operation
  {
    flat_set< account_name_type > required_owner_auths;
    flat_set< account_name_type > required_active_auths;
    flat_set< account_name_type > required_posting_auths;
    vector< authority >           required_auths;

    custom_id_type                id; ///< must be less than 32 characters long
    vector< char >                data;

    void validate()const;
    void get_required_owner_authorities( flat_set<account_name_type>& a )const{ for( const auto& i : required_owner_auths ) a.insert(i); }
    void get_required_active_authorities( flat_set<account_name_type>& a )const{ for( const auto& i : required_active_auths ) a.insert(i); }
    void get_required_posting_authorities( flat_set<account_name_type>& a )const{ for( const auto& i : required_posting_auths ) a.insert(i); }
    void get_required_authorities( vector< authority >& a )const{ for( const auto& i : required_auths ) a.push_back( i ); }
  };


  /**
    *  Feeds can only be published by the top N witnesses which are included in every round and are
    *  used to define the exchange rate between HIVE and the dollar (represented by HBD).
    */
  struct feed_publish_operation : public base_operation
  {
    account_name_type publisher;
    price             exchange_rate;

    void  validate()const;
    void  get_required_active_authorities( flat_set<account_name_type>& a )const{ a.insert(publisher); }
  };


  /**
    *  This operation instructs the blockchain to start a conversion of HBD to HIVE.
    *  The funds are deposited after HIVE_CONVERSION_DELAY
    */
  struct convert_operation : public base_operation
  {
    account_name_type owner;
    uint32_t          requestid = 0;
    asset             amount; //in HBD

    void  validate()const;
    void  get_required_active_authorities( flat_set<account_name_type>& a )const{ a.insert(owner); }
  };

  /**
    *  Similar to convert_operation, this operation instructs the blockchain to convert HIVE to HBD.
    *  The operation is performed after HIVE_COLLATERALIZED_CONVERSION_DELAY, but owner gets HBD
    *  immediately. The price risk is cussioned by extra HIVE (see HIVE_COLLATERAL_RATIO). After actual
    *  conversion takes place the excess HIVE is returned to the owner.
    */
  struct collateralized_convert_operation : public base_operation
  {
    account_name_type owner;
    uint32_t          requestid = 0;
    asset             amount; //in HIVE

    void  validate()const;
    void  get_required_active_authorities( flat_set<account_name_type>& a )const { a.insert( owner ); }
  };


  /**
    * This operation creates a limit order and matches it against existing open orders.
    */
  struct limit_order_create_operation : public base_operation
  {
    account_name_type owner;
    uint32_t          orderid = 0; /// an ID assigned by owner, must be unique
    asset             amount_to_sell;
    asset             min_to_receive;
    bool              fill_or_kill = false;
    time_point_sec    expiration = time_point_sec::maximum();

    void  validate()const;
    void  get_required_active_authorities( flat_set<account_name_type>& a )const{ a.insert(owner); }

    price             get_price()const { return price( amount_to_sell, min_to_receive ); }

    pair< asset_symbol_type, asset_symbol_type > get_market()const
    {
      return amount_to_sell.symbol < min_to_receive.symbol ?
            std::make_pair(amount_to_sell.symbol, min_to_receive.symbol) :
            std::make_pair(min_to_receive.symbol, amount_to_sell.symbol);
    }
  };


  /**
    *  This operation is identical to limit_order_create except it serializes the price rather
    *  than calculating it from other fields.
    */
  struct limit_order_create2_operation : public base_operation
  {
    account_name_type owner;
    uint32_t          orderid = 0; /// an ID assigned by owner, must be unique
    asset             amount_to_sell;
    bool              fill_or_kill = false;
    price             exchange_rate;
    time_point_sec    expiration = time_point_sec::maximum();

    void  validate()const;
    void  get_required_active_authorities( flat_set<account_name_type>& a )const{ a.insert(owner); }

    price             get_price()const { return exchange_rate; }

    pair< asset_symbol_type, asset_symbol_type > get_market()const
    {
      return exchange_rate.base.symbol < exchange_rate.quote.symbol ?
            std::make_pair(exchange_rate.base.symbol, exchange_rate.quote.symbol) :
            std::make_pair(exchange_rate.quote.symbol, exchange_rate.base.symbol);
    }
  };


  /**
    *  Cancels an order and returns the balance to owner.
    */
  struct limit_order_cancel_operation : public base_operation
  {
    account_name_type owner;
    uint32_t          orderid = 0;

    void  validate()const;
    void  get_required_active_authorities( flat_set<account_name_type>& a )const{ a.insert(owner); }
  };


  struct pow
  {
    public_key_type worker;
    digest_type     input;
    signature_type  signature;
    digest_type     work;

    void create( const fc::ecc::private_key& w, const digest_type& i );
    void validate()const;
  };


  struct pow_operation : public base_operation
  {
    account_name_type worker_account;
    block_id_type     block_id;
    uint64_t          nonce = 0;
    pow               work;
    legacy_chain_properties  props;

    void validate()const;
    fc::sha256 work_input()const;

    const account_name_type& get_worker_account()const { return worker_account; }

    /** there is no need to verify authority, the proof of work is sufficient */
    void get_required_active_authorities( flat_set<account_name_type>& a )const{  }
  };


  struct pow2_input
  {
    account_name_type worker_account;
    block_id_type     prev_block;
    uint64_t          nonce = 0;
  };


  struct pow2
  {
    pow2_input        input;
    uint32_t          pow_summary = 0;

    void create( const block_id_type& prev_block, const account_name_type& account_name, uint64_t nonce );
    void validate()const;
  };

  struct equihash_pow
  {
    pow2_input           input;
    fc::equihash::proof  proof;
    block_id_type        prev_block;
    uint32_t             pow_summary = 0;

    void create( const block_id_type& recent_block, const account_name_type& account_name, uint32_t nonce );
    void validate() const;
  };

  typedef fc::static_variant< pow2, equihash_pow > pow2_work;

  struct pow2_operation : public base_operation
  {
    pow2_work                     work;
    optional< public_key_type >   new_owner_key;
    legacy_chain_properties       props;

    void validate()const;

    void get_required_active_authorities( flat_set<account_name_type>& a )const;

    void get_required_authorities( vector< authority >& a )const
    {
      if( new_owner_key )
      {
        a.push_back( authority( 1, *new_owner_key, 1 ) );
      }
    }
  };

  /**
    * All account recovery requests come from a listed recovery account. This
    * is secure based on the assumption that only a trusted account should be
    * a recovery account. It is the responsibility of the recovery account to
    * verify the identity of the account holder of the account to recover by
    * whichever means they have agreed upon. The blockchain assumes identity
    * has been verified when this operation is broadcast.
    *
    * This operation creates an account recovery request which the account to
    * recover has 24 hours to respond to before the request expires and is
    * invalidated.
    *
    * There can only be one active recovery request per account at any one time.
    * Pushing this operation for an account to recover when it already has
    * an active request will either update the request to a new new owner authority
    * and extend the request expiration to 24 hours from the current head block
    * time or it will delete the request. To cancel a request, simply set the
    * weight threshold of the new owner authority to 0, making it an open authority.
    *
    * Additionally, the new owner authority must be satisfiable. In other words,
    * the sum of the key weights must be greater than or equal to the weight
    * threshold.
    *
    * This operation only needs to be signed by the the recovery account.
    * The account to recover confirms its identity to the blockchain in
    * the recover account operation.
    */
  struct request_account_recovery_operation : public base_operation
  {
    account_name_type recovery_account;       ///< The recovery account is listed as the recovery account on the account to recover.

    account_name_type account_to_recover;     ///< The account to recover. This is likely due to a compromised owner authority.

    authority         new_owner_authority;    ///< The new owner authority the account to recover wishes to have. This is secret
                                ///< known by the account to recover and will be confirmed in a recover_account_operation

    extensions_type   extensions;             ///< Extensions. Not currently used.

    void get_required_active_authorities( flat_set<account_name_type>& a )const{ a.insert( recovery_account ); }

    void validate() const;
  };


  /**
    * Recover an account to a new authority using a previous authority and verification
    * of the recovery account as proof of identity. This operation can only succeed
    * if there was a recovery request sent by the account's recover account.
    *
    * In order to recover the account, the account holder must provide proof
    * of past ownership and proof of identity to the recovery account. Being able
    * to satisfy an owner authority that was used in the past 30 days is sufficient
    * to prove past ownership. The get_owner_history function in the database API
    * returns past owner authorities that are valid for account recovery.
    *
    * Proving identity is an off chain contract between the account holder and
    * the recovery account. The recovery request contains a new authority which
    * must be satisfied by the account holder to regain control. The actual process
    * of verifying authority may become complicated, but that is an application
    * level concern, not a blockchain concern.
    *
    * This operation requires both the past and future owner authorities in the
    * operation because neither of them can be derived from the current chain state.
    * The operation must be signed by keys that satisfy both the new owner authority
    * and the recent owner authority. Failing either fails the operation entirely.
    *
    * If a recovery request was made inadvertantly, the account holder should
    * contact the recovery account to have the request deleted.
    *
    * The two setp combination of the account recovery request and recover is
    * safe because the recovery account never has access to secrets of the account
    * to recover. They simply act as an on chain endorsement of off chain identity.
    * In other systems, a fork would be required to enforce such off chain state.
    * Additionally, an account cannot be permanently recovered to the wrong account.
    * While any owner authority from the past 30 days can be used, including a compromised
    * authority, the account can be continually recovered until the recovery account
    * is confident a combination of uncompromised authorities were used to
    * recover the account. The actual process of verifying authority may become
    * complicated, but that is an application level concern, not the blockchain's
    * concern.
    */
  struct recover_account_operation : public base_operation
  {
    account_name_type account_to_recover;        ///< The account to be recovered

    authority         new_owner_authority;       ///< The new owner authority as specified in the request account recovery operation.

    authority         recent_owner_authority;    ///< A previous owner authority that the account holder will use to prove past ownership of the account to be recovered.

    extensions_type   extensions;                ///< Extensions. Not currently used.

    void get_required_authorities( vector< authority >& a )const
    {
      a.push_back( new_owner_authority );
      a.push_back( recent_owner_authority );
    }

    void validate() const;
  };


  /**
    *  This operation allows recovery_accoutn to change account_to_reset's owner authority to
    *  new_owner_authority after 60 days of inactivity.
    */
  struct reset_account_operation : public base_operation {
    account_name_type reset_account;
    account_name_type account_to_reset;
    authority         new_owner_authority;

    void get_required_active_authorities( flat_set<account_name_type>& a )const { a.insert( reset_account ); }
    void validate()const;
  };

  /**
    * This operation allows 'account' owner to control which account has the power
    * to execute the 'reset_account_operation' after 60 days.
    */
  struct set_reset_account_operation : public base_operation {
    account_name_type account;
    account_name_type current_reset_account;
    account_name_type reset_account;
    void validate()const;
    void get_required_owner_authorities( flat_set<account_name_type>& a )const
    {
      if( current_reset_account.size() )
        a.insert( account );
    }

    void get_required_posting_authorities( flat_set<account_name_type>& a )const
    {
      if( !current_reset_account.size() )
        a.insert( account );
    }
  };


  /**
    * Each account lists another account as their recovery account.
    * The recovery account has the ability to create account_recovery_requests
    * for the account to recover. An account can change their recovery account
    * at any time with a 30 day delay. This delay is to prevent
    * an attacker from changing the recovery account to a malicious account
    * during an attack. These 30 days match the 30 days that an
    * owner authority is valid for recovery purposes.
    *
    * On account creation the recovery account is set either to the creator of
    * the account (The account that pays the creation fee and is a signer on the transaction)
    * or to the empty string if the account was mined. An account with no recovery
    * has the top voted witness as a recovery account, at the time the recover
    * request is created. Note: This does mean the effective recovery account
    * of an account with no listed recovery account can change at any time as
    * witness vote weights. The top voted witness is explicitly the most trusted
    * witness according to stake.
    */
  struct change_recovery_account_operation : public base_operation
  {
    account_name_type account_to_recover;     ///< The account that would be recovered in case of compromise
    account_name_type new_recovery_account;   ///< The account that creates the recover request
    extensions_type   extensions;             ///< Extensions. Not currently used.

    void get_required_owner_authorities( flat_set<account_name_type>& a )const{ a.insert( account_to_recover ); }
    void validate() const;
  };


  struct transfer_to_savings_operation : public base_operation {
    account_name_type from;
    account_name_type to;
    asset             amount;
    string            memo;

    void get_required_active_authorities( flat_set<account_name_type>& a )const{ a.insert( from ); }
    void validate() const;
  };


  struct transfer_from_savings_operation : public base_operation {
    account_name_type from;
    uint32_t          request_id = 0;
    account_name_type to;
    asset             amount;
    string            memo;

    void get_required_active_authorities( flat_set<account_name_type>& a )const{ a.insert( from ); }
    void validate() const;
  };


  struct cancel_transfer_from_savings_operation : public base_operation {
    account_name_type from;
    uint32_t          request_id = 0;

    void get_required_active_authorities( flat_set<account_name_type>& a )const{ a.insert( from ); }
    void validate() const;
  };


  struct decline_voting_rights_operation : public base_operation
  {
    account_name_type account;
    bool              decline = true;

    void get_required_owner_authorities( flat_set<account_name_type>& a )const{ a.insert( account ); }
    void validate() const;
  };

  struct claim_reward_balance_operation : public base_operation
  {
    account_name_type account;
    asset             reward_hive;
    asset             reward_hbd;
    asset             reward_vests;

    void get_required_posting_authorities( flat_set< account_name_type >& a )const{ a.insert( account ); }
    void validate() const;
  };

#ifdef HIVE_ENABLE_SMT
  /** Differs with original operation with extensions field and a container of tokens that will
    *  be rewarded to an account. See discussion in issue #1859
    */
  struct claim_reward_balance2_operation : public base_operation
  {
    account_name_type account;
    extensions_type   extensions;

    /** \warning The contents of this container is required to be unique and sorted
      *  (both by asset symbol) in ascending order. Otherwise operation validation will fail.
      */
    vector< asset > reward_tokens;

    void get_required_posting_authorities( flat_set< account_name_type >& a )const{ a.insert( account ); }
    void validate() const;
  };
#endif

  /**
    * Delegate vesting shares from one account to the other. The vesting shares are still owned
    * by the original account, but content voting rights and bandwidth allocation are transferred
    * to the receiving account. This sets the delegation to `vesting_shares`, increasing it or
    * decreasing it as needed. (i.e. a delegation of 0 removes the delegation)
    *
    * When a delegation is removed the shares are placed in limbo for a week to prevent a satoshi
    * of VESTS from voting on the same content twice.
    */
  struct delegate_vesting_shares_operation : public base_operation
  {
    account_name_type delegator;        ///< The account delegating vesting shares
    account_name_type delegatee;        ///< The account receiving vesting shares
    asset             vesting_shares;   ///< The amount of vesting shares delegated

    void get_required_active_authorities( flat_set< account_name_type >& a ) const { a.insert( delegator ); }
    void validate() const;
  };

  struct recurrent_transfer_pair_id
  {
    uint8_t pair_id = 0;
  };

  typedef static_variant<
  void_t,
  recurrent_transfer_pair_id
  > recurrent_transfer_extension;

  typedef flat_set< recurrent_transfer_extension > recurrent_transfer_extensions_type;

  /**
    * @ingroup operations
    *
    * @brief Creates/updates/removes a recurrent transfer (Transfers any liquid asset (nonvesting) every fixed amount of time from one account to another)
    * If amount is set to 0, the recurrent transfer will be deleted
    * If there is already a recurrent transfer matching from and to, the recurrent transfer will be updated
    */
  struct recurrent_transfer_operation : public base_operation
  {
    account_name_type from;
    /// Account to transfer asset to
    account_name_type to;
    /// The amount of asset to transfer from @ref from to @ref to
    asset             amount;

    string            memo;
    /// How often will the payment be triggered, unit: hours
    uint16_t          recurrence = 0;

    // How many times the recurrent payment will be executed
    uint16_t          executions = 0;

    /// Extensions, only recurrent_transfer_pair_id is supported so far
    recurrent_transfer_extensions_type   extensions;

    void              validate()const;
    void get_required_active_authorities( flat_set<account_name_type>& a )const{ a.insert(from); }
  };

  struct witness_block_approve_operation : public base_operation
  {
    account_name_type witness;
    block_id_type     block_id;

    void validate()const;
    void get_required_witness_signatures( flat_set<account_name_type>& a )const{ a.insert(witness); }
  };

  } } // hive::protocol


namespace fc
{
  using hive::protocol::comment_options_extension;
  template<>
  struct serialization_functor< comment_options_extension >
  {
    bool operator()( const fc::variant& v, comment_options_extension& s ) const
    {
      return extended_serialization_functor< comment_options_extension >().serialize( v, s );
    }
  };

  template<>
  struct variant_creator_functor< comment_options_extension >
  {
    template<typename T>
    fc::variant operator()( const T& v ) const
    {
      return extended_variant_creator_functor< comment_options_extension >().create( v );
    }
  };

  using hive::protocol::pow2_work;
  template<>
  struct serialization_functor< pow2_work >
  {
    bool operator()( const fc::variant& v, pow2_work& s ) const
    {
      return extended_serialization_functor< pow2_work >().serialize( v, s );
    }
  };

  template<>
  struct variant_creator_functor< pow2_work >
  {
    template<typename T>
    fc::variant operator()( const T& v ) const
    {
      return extended_variant_creator_functor< pow2_work >().create( v );
    }
  };
}

FC_REFLECT( hive::protocol::transfer_to_savings_operation, (from)(to)(amount)(memo) )
FC_REFLECT( hive::protocol::transfer_from_savings_operation, (from)(request_id)(to)(amount)(memo) )
FC_REFLECT( hive::protocol::cancel_transfer_from_savings_operation, (from)(request_id) )

FC_REFLECT( hive::protocol::reset_account_operation, (reset_account)(account_to_reset)(new_owner_authority) )
FC_REFLECT( hive::protocol::set_reset_account_operation, (account)(current_reset_account)(reset_account) )


FC_REFLECT( hive::protocol::convert_operation, (owner)(requestid)(amount) )
FC_REFLECT( hive::protocol::collateralized_convert_operation, (owner)(requestid)(amount) )
FC_REFLECT( hive::protocol::feed_publish_operation, (publisher)(exchange_rate) )
FC_REFLECT( hive::protocol::pow, (worker)(input)(signature)(work) )
FC_REFLECT( hive::protocol::pow2, (input)(pow_summary) )
FC_REFLECT( hive::protocol::pow2_input, (worker_account)(prev_block)(nonce) )
FC_REFLECT( hive::protocol::equihash_pow, (input)(proof)(prev_block)(pow_summary) )
FC_REFLECT( hive::protocol::legacy_chain_properties,
        (account_creation_fee)
        (maximum_block_size)
        (hbd_interest_rate)
        )

FC_REFLECT_TYPENAME( hive::protocol::pow2_work )
FC_REFLECT( hive::protocol::pow_operation, (worker_account)(block_id)(nonce)(work)(props) )
FC_REFLECT( hive::protocol::pow2_operation, (work)(new_owner_key)(props) )

FC_REFLECT( hive::protocol::account_create_operation,
        (fee)
        (creator)
        (new_account_name)
        (owner)
        (active)
        (posting)
        (memo_key)
        (json_metadata) )

FC_REFLECT( hive::protocol::account_create_with_delegation_operation,
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

FC_REFLECT( hive::protocol::account_update_operation,
        (account)
        (owner)
        (active)
        (posting)
        (memo_key)
        (json_metadata) )

FC_REFLECT( hive::protocol::account_update2_operation,
        (account)
        (owner)
        (active)
        (posting)
        (memo_key)
        (json_metadata)
        (posting_json_metadata)
        (extensions) )

FC_REFLECT( hive::protocol::transfer_operation, (from)(to)(amount)(memo) )
FC_REFLECT( hive::protocol::transfer_to_vesting_operation, (from)(to)(amount) )
FC_REFLECT( hive::protocol::withdraw_vesting_operation, (account)(vesting_shares) )
FC_REFLECT( hive::protocol::set_withdraw_vesting_route_operation, (from_account)(to_account)(percent)(auto_vest) )
FC_REFLECT( hive::protocol::witness_update_operation, (owner)(url)(block_signing_key)(props)(fee) )
FC_REFLECT( hive::protocol::witness_set_properties_operation, (owner)(props)(extensions) )
FC_REFLECT( hive::protocol::account_witness_vote_operation, (account)(witness)(approve) )
FC_REFLECT( hive::protocol::account_witness_proxy_operation, (account)(proxy) )
FC_REFLECT( hive::protocol::comment_operation, (parent_author)(parent_permlink)(author)(permlink)(title)(body)(json_metadata) )
FC_REFLECT( hive::protocol::vote_operation, (voter)(author)(permlink)(weight) )
FC_REFLECT( hive::protocol::custom_operation, (required_auths)(id)(data) )
FC_REFLECT( hive::protocol::custom_json_operation, (required_auths)(required_posting_auths)(id)(json) )
FC_REFLECT( hive::protocol::custom_binary_operation, (required_owner_auths)(required_active_auths)(required_posting_auths)(required_auths)(id)(data) )
FC_REFLECT( hive::protocol::limit_order_create_operation, (owner)(orderid)(amount_to_sell)(min_to_receive)(fill_or_kill)(expiration) )
FC_REFLECT( hive::protocol::limit_order_create2_operation, (owner)(orderid)(amount_to_sell)(exchange_rate)(fill_or_kill)(expiration) )
FC_REFLECT( hive::protocol::limit_order_cancel_operation, (owner)(orderid) )

FC_REFLECT( hive::protocol::delete_comment_operation, (author)(permlink) );

FC_REFLECT( hive::protocol::beneficiary_route_type, (account)(weight) )
FC_REFLECT( hive::protocol::comment_payout_beneficiaries, (beneficiaries) )

#ifdef HIVE_ENABLE_SMT
FC_REFLECT( hive::protocol::votable_asset_info_v1, (max_accepted_payout)(allow_curation_rewards) )
FC_REFLECT( hive::protocol::allowed_vote_assets, (votable_assets) )
#endif

FC_REFLECT_TYPENAME( hive::protocol::comment_options_extension )
FC_REFLECT( hive::protocol::comment_options_operation, (author)(permlink)(max_accepted_payout)(percent_hbd)(allow_votes)(allow_curation_rewards)(extensions) )

FC_REFLECT( hive::protocol::escrow_transfer_operation, (from)(to)(hbd_amount)(hive_amount)(escrow_id)(agent)(fee)(json_meta)(ratification_deadline)(escrow_expiration) );
FC_REFLECT( hive::protocol::escrow_approve_operation, (from)(to)(agent)(who)(escrow_id)(approve) );
FC_REFLECT( hive::protocol::escrow_dispute_operation, (from)(to)(agent)(who)(escrow_id) );
FC_REFLECT( hive::protocol::escrow_release_operation, (from)(to)(agent)(who)(receiver)(escrow_id)(hbd_amount)(hive_amount) );
FC_REFLECT( hive::protocol::claim_account_operation, (creator)(fee)(extensions) );
FC_REFLECT( hive::protocol::create_claimed_account_operation, (creator)(new_account_name)(owner)(active)(posting)(memo_key)(json_metadata)(extensions) );
FC_REFLECT( hive::protocol::request_account_recovery_operation, (recovery_account)(account_to_recover)(new_owner_authority)(extensions) );
FC_REFLECT( hive::protocol::recover_account_operation, (account_to_recover)(new_owner_authority)(recent_owner_authority)(extensions) );
FC_REFLECT( hive::protocol::change_recovery_account_operation, (account_to_recover)(new_recovery_account)(extensions) );
FC_REFLECT( hive::protocol::decline_voting_rights_operation, (account)(decline) );
FC_REFLECT( hive::protocol::claim_reward_balance_operation, (account)(reward_hive)(reward_hbd)(reward_vests) );
#ifdef HIVE_ENABLE_SMT
FC_REFLECT( hive::protocol::claim_reward_balance2_operation, (account)(extensions)(reward_tokens) )
#endif
FC_REFLECT( hive::protocol::delegate_vesting_shares_operation, (delegator)(delegatee)(vesting_shares) );
FC_REFLECT( hive::protocol::recurrent_transfer_operation, (from)(to)(amount)(memo)(recurrence)(executions)(extensions) );
FC_REFLECT( hive::protocol::witness_block_approve_operation, (witness)(block_id) );
FC_REFLECT( hive::protocol::recurrent_transfer_pair_id, (pair_id) );

