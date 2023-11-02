#include <hive/protocol/hive_operations.hpp>

#include <fc/macros.hpp>
#include <fc/io/json.hpp>
#include <fc/macros.hpp>

#include <chrono>
#include <locale>

namespace hive { namespace protocol {
  namespace
  {
    // the fast_is_valid() check verifies that the document is valid utf-8 and valid json.
    // the old is_valid() test is slower and let some bad json through.  For now, we're falling
    // back to the old parser which will allow us to replay blocks that the old is_valid() allowed
    // but shouldn't have.
    void validate_json_with_fallback(const std::string& json)
    {
      if (!fc::json::fast_is_valid(json))
      {
        FC_ASSERT(fc::is_utf8(json), "JSON not formatted in UTF8");
        FC_ASSERT(fc::json::is_valid(json, false /* full_json_validation */), "JSON is not valid JSON");
      }
    }
  }

  void validate_auth_size( const authority& a )
  {
    size_t size = a.account_auths.size() + a.key_auths.size();
    FC_ASSERT( size <= HIVE_MAX_AUTHORITY_MEMBERSHIP,
      "Authority membership exceeded. Max: ${max} Current: ${n}", ("max", HIVE_MAX_AUTHORITY_MEMBERSHIP)("n", size) );
  }

  void account_create_operation::validate() const
  {
    validate_account_name( new_account_name );
    FC_ASSERT( is_asset_type( fee, HIVE_SYMBOL ), "Account creation fee must be HIVE" );
    owner.validate();
    active.validate();

    if (!json_metadata.empty())
      validate_json_with_fallback(json_metadata);

    FC_ASSERT( fee >= asset( 0, HIVE_SYMBOL ), "Account creation fee cannot be negative" );
  }

  void account_create_with_delegation_operation::validate() const
  {
    validate_account_name( new_account_name );
    validate_account_name( creator );
    FC_ASSERT( is_asset_type( fee, HIVE_SYMBOL ), "Account creation fee must be HIVE" );
    FC_ASSERT( is_asset_type( delegation, VESTS_SYMBOL ), "Delegation must be VESTS" );

    owner.validate();
    active.validate();
    posting.validate();

    if (!json_metadata.empty())
      validate_json_with_fallback(json_metadata);

    FC_ASSERT( fee >= asset( 0, HIVE_SYMBOL ), "Account creation fee cannot be negative" );
    FC_ASSERT( delegation >= asset( 0, VESTS_SYMBOL ), "Delegation cannot be negative" );
  }

  void account_update_operation::validate() const
  {
    validate_account_name( account );
    /*if( owner )
      owner->validate();
    if( active )
      active->validate();
    if( posting )
      posting->validate();*/

    if (!json_metadata.empty())
      validate_json_with_fallback(json_metadata);
  }

  void account_update2_operation::validate() const
  {
    validate_account_name( account );

    if (!json_metadata.empty())
      validate_json_with_fallback(json_metadata);

    if (!posting_json_metadata.empty())
      validate_json_with_fallback(posting_json_metadata);
  }

  void comment_operation::validate() const
  {
    FC_ASSERT( title.size() < HIVE_COMMENT_TITLE_LIMIT,
      "Title size limit exceeded. Max: ${max} Current: ${n}", ("max", HIVE_COMMENT_TITLE_LIMIT - 1)("n", title.size()) );
    FC_ASSERT( fc::is_utf8( title ), "Title not formatted in UTF8" );
    FC_ASSERT( body.size() > 0, "Body is empty" );
    FC_ASSERT( fc::is_utf8( body ), "Body not formatted in UTF8" );


    if( parent_author.size() )
      validate_account_name( parent_author );
    validate_account_name( author );
    validate_permlink( parent_permlink );
    validate_permlink( permlink );

    if (!json_metadata.empty())
      validate_json_with_fallback(json_metadata);
  }

  struct comment_options_extension_validate_visitor
  {
    typedef void result_type;

#ifdef HIVE_ENABLE_SMT
    void operator()( const allowed_vote_assets& va) const
    {
      va.validate();
    }
#endif
    void operator()( const comment_payout_beneficiaries& cpb ) const
    {
      cpb.validate();
    }
  };

  void comment_payout_beneficiaries::validate()const
  {
    uint32_t sum = 0;

    FC_ASSERT( beneficiaries.size(), "Must specify at least one beneficiary" );
    FC_ASSERT( beneficiaries.size() < HIVE_BENEFICIARY_LIMIT,
      "Cannot specify more than ${max} beneficiaries.", ("max", HIVE_BENEFICIARY_LIMIT - 1) ); // Require size serialization fits in one byte.

    validate_account_name( beneficiaries[0].account );
    FC_ASSERT( beneficiaries[0].weight <= HIVE_100_PERCENT, "Cannot allocate more than 100% of rewards to one account" );
    sum += beneficiaries[0].weight;
    FC_ASSERT( sum <= HIVE_100_PERCENT, "Cannot allocate more than 100% of rewards to a comment" ); // Have to check incrementally to avoid overflow

    for( size_t i = 1; i < beneficiaries.size(); i++ )
    {
      validate_account_name( beneficiaries[i].account );
      FC_ASSERT( beneficiaries[i].weight <= HIVE_100_PERCENT, "Cannot allocate more than 100% of rewards to one account" );
      sum += beneficiaries[i].weight;
      FC_ASSERT( sum <= HIVE_100_PERCENT, "Cannot allocate more than 100% of rewards to a comment" ); // Have to check incrementally to avoid overflow
      FC_ASSERT( beneficiaries[i - 1] < beneficiaries[i], "Beneficiaries must be specified in sorted order (account ascending)" );
    }
  }

  void comment_options_operation::validate()const
  {
    validate_account_name( author );
    FC_ASSERT( percent_hbd <= HIVE_100_PERCENT, "Percent cannot exceed 100%" );
    FC_ASSERT( max_accepted_payout.symbol == HBD_SYMBOL, "Max accepted payout must be in HBD" );
    FC_ASSERT( max_accepted_payout.amount.value >= 0, "Cannot accept less than 0 payout" );
    validate_permlink( permlink );
    for( auto& e : extensions )
      e.visit( comment_options_extension_validate_visitor() );
  }

  void delete_comment_operation::validate()const
  {
    validate_permlink( permlink );
    validate_account_name( author );
  }

  void claim_account_operation::validate()const
  {
    validate_account_name( creator );
    FC_ASSERT( is_asset_type( fee, HIVE_SYMBOL ), "Account creation fee must be HIVE" );
    FC_ASSERT( fee >= asset( 0, HIVE_SYMBOL ), "Account creation fee cannot be negative" );
    FC_ASSERT( fee <= asset( HIVE_MAX_ACCOUNT_CREATION_FEE, HIVE_SYMBOL ), "Account creation fee cannot be too large" );

    FC_ASSERT( extensions.size() == 0, "There are no extensions for claim_account_operation." );
  }

  void create_claimed_account_operation::validate()const
  {
    validate_account_name( creator );
    validate_account_name( new_account_name );
    owner.validate();
    active.validate();
    posting.validate();
    validate_auth_size( owner );
    validate_auth_size( active );
    validate_auth_size( posting );

    if (!json_metadata.empty())
      validate_json_with_fallback(json_metadata);

    FC_ASSERT( extensions.size() == 0, "There are no extensions for create_claimed_account_operation." );
  }

  void vote_operation::validate() const
  {
    validate_account_name( voter );
    validate_account_name( author );\
    FC_ASSERT( abs(weight) <= HIVE_100_PERCENT, "Weight is not a HIVE percentage" );
    validate_permlink( permlink );
  }

  void transfer_operation::validate() const
  { try {
    validate_account_name( from );
    validate_account_name( to );
    FC_ASSERT( amount.symbol.is_vesting() == false, "Transfer of vesting is not allowed." );
    FC_ASSERT( amount.amount > 0, "Cannot transfer a negative amount (aka: stealing)" );
    FC_ASSERT( memo.size() < HIVE_MAX_MEMO_SIZE, "Memo is too large" );
    FC_ASSERT( fc::is_utf8( memo ), "Memo is not UTF8" );
  } FC_CAPTURE_AND_RETHROW( (*this) ) }

  void transfer_to_vesting_operation::validate() const
  {
    validate_account_name( from );
    FC_ASSERT( amount.symbol == HIVE_SYMBOL ||
            ( amount.symbol.space() == asset_symbol_type::smt_nai_space && amount.symbol.is_vesting() == false ),
            "Amount must be HIVE or SMT liquid" );
    if ( to != account_name_type() ) validate_account_name( to );
    FC_ASSERT( amount.amount > 0, "Must transfer a nonzero amount" );
  }

  void withdraw_vesting_operation::validate() const
  {
    validate_account_name( account );
    FC_ASSERT( is_asset_type( vesting_shares, VESTS_SYMBOL), "Amount must be VESTS"  );
  }

  void set_withdraw_vesting_route_operation::validate() const
  {
    validate_account_name( from_account );
    validate_account_name( to_account );
    FC_ASSERT( 0 <= percent && percent <= HIVE_100_PERCENT, "Percent must be valid HIVE percent" );
  }

  void witness_update_operation::validate() const
  {
    validate_account_name( owner );

    FC_ASSERT( url.size() <= HIVE_MAX_WITNESS_URL_LENGTH, "URL is too long" );

    FC_ASSERT( url.size() > 0, "URL size must be greater than 0" );
    FC_ASSERT( fc::is_utf8( url ), "URL is not valid UTF8" );
    FC_ASSERT( fee >= asset( 0, HIVE_SYMBOL ), "Fee cannot be negative" );
    props.validate< false >();
  }

  void witness_set_properties_operation::validate() const
  {
    validate_account_name( owner );

    // current signing key must be present
    FC_ASSERT( props.find( "key" ) != props.end(), "No signing key provided" );

    auto itr = props.find( "account_creation_fee" );
    if( itr != props.end() )
    {
      asset account_creation_fee;
      fc::raw::unpack_from_vector( itr->second, account_creation_fee );
      FC_ASSERT( account_creation_fee.symbol == HIVE_SYMBOL, "account_creation_fee must be in HIVE" );
      FC_ASSERT( account_creation_fee.amount >= HIVE_MIN_ACCOUNT_CREATION_FEE, "account_creation_fee smaller than minimum account creation fee" );
    }

    itr = props.find( "maximum_block_size" );
    if( itr != props.end() )
    {
      uint32_t maximum_block_size = 0u;
      fc::raw::unpack_from_vector( itr->second, maximum_block_size );
      FC_ASSERT( maximum_block_size >= HIVE_MIN_BLOCK_SIZE_LIMIT, "maximum_block_size smaller than minimum max block size" );
    }

    itr = props.find( "sbd_interest_rate" );
    if( itr == props.end() )
      itr = props.find( "hbd_interest_rate" );

    if( itr != props.end() )
    {
      uint16_t hbd_interest_rate = 0u;
      fc::raw::unpack_from_vector( itr->second, hbd_interest_rate );
      FC_ASSERT( hbd_interest_rate >= 0, "hbd_interest_rate must be positive" );
      FC_ASSERT( hbd_interest_rate <= HIVE_100_PERCENT, "hbd_interest_rate must not exceed 100%" );
    }

    itr = props.find( "new_signing_key" );
    if( itr != props.end() )
    {
      public_key_type signing_key;
      fc::raw::unpack_from_vector( itr->second, signing_key );
      FC_UNUSED( signing_key ); // This tests the deserialization of the key
    }

    itr = props.find( "sbd_exchange_rate" );
    if( itr == props.end() )
      itr = props.find( "hbd_exchange_rate" );

    if( itr != props.end() )
    {
      price hbd_exchange_rate;
      fc::raw::unpack_from_vector( itr->second, hbd_exchange_rate );
      FC_ASSERT( ( is_asset_type( hbd_exchange_rate.base, HBD_SYMBOL ) && is_asset_type( hbd_exchange_rate.quote, HIVE_SYMBOL ) ),
        "Price feed must be a HIVE/HBD price" );
      hbd_exchange_rate.validate();
    }

    itr = props.find( "url" );
    if( itr != props.end() )
    {
      std::string url;
      fc::raw::unpack_from_vector< std::string >( itr->second, url );

      FC_ASSERT( url.size() <= HIVE_MAX_WITNESS_URL_LENGTH, "URL is too long" );
      FC_ASSERT( url.size() > 0, "URL size must be greater than 0" );
      FC_ASSERT( fc::is_utf8( url ), "URL is not valid UTF8" );
    }

    itr = props.find( "account_subsidy_budget" );
    if( itr != props.end() )
    {
      int32_t account_subsidy_budget  = 0u;
      fc::raw::unpack_from_vector( itr->second, account_subsidy_budget ); // Checks that the value can be deserialized
      FC_ASSERT( account_subsidy_budget >= HIVE_RD_MIN_BUDGET, "Budget must be at least ${n}", ("n", HIVE_RD_MIN_BUDGET) );
      FC_ASSERT( account_subsidy_budget <= HIVE_RD_MAX_BUDGET, "Budget must be at most ${n}", ("n", HIVE_RD_MAX_BUDGET) );
    }

    itr = props.find( "account_subsidy_decay" );
    if( itr != props.end() )
    {
      uint32_t account_subsidy_decay = 0u;
      fc::raw::unpack_from_vector( itr->second, account_subsidy_decay ); // Checks that the value can be deserialized
      FC_ASSERT( account_subsidy_decay >= HIVE_RD_MIN_DECAY, "Decay must be at least ${n}", ("n", HIVE_RD_MIN_DECAY) );
      FC_ASSERT( account_subsidy_decay <= HIVE_RD_MAX_DECAY, "Decay must be at most ${n}", ("n", HIVE_RD_MAX_DECAY) );
    }
  }

  void account_witness_vote_operation::validate() const
  {
    validate_account_name( account );
    validate_account_name( witness );
  }

  void account_witness_proxy_operation::validate() const
  {
    validate_account_name( account );
    if( !is_clearing_proxy() )
      validate_account_name( proxy );
    FC_ASSERT( proxy != account, "Cannot proxy to self" );
  }

  void custom_operation::validate() const {
    /// required auth accounts are the ones whose bandwidth is consumed
    FC_ASSERT( required_auths.size() > 0, "at least one account must be specified" );
  }
  void custom_json_operation::validate() const {
    /// required auth accounts are the ones whose bandwidth is consumed
    FC_ASSERT( (required_auths.size() + required_posting_auths.size()) > 0, "at least one account must be specified" );
    FC_ASSERT( id.size() <= HIVE_CUSTOM_OP_ID_MAX_LENGTH,
      "Operation ID length exceeded. Max: ${max} Current: ${n}", ("max", HIVE_CUSTOM_OP_ID_MAX_LENGTH)("n", id.size()) );
    validate_json_with_fallback(json);
  }
  void custom_binary_operation::validate() const {
    /// required auth accounts are the ones whose bandwidth is consumed
    FC_ASSERT( (required_owner_auths.size() + required_active_auths.size() + required_posting_auths.size()) > 0, "at least one account must be specified" );
    FC_ASSERT( id.size() <= HIVE_CUSTOM_OP_ID_MAX_LENGTH,
      "Operation ID length exceeded. Max: ${max} Current: ${n}", ("max", HIVE_CUSTOM_OP_ID_MAX_LENGTH)("n", id.size()) );
    for( const auto& a : required_auths ) a.validate();
  }


  fc::sha256 pow_operation::work_input()const
  {
    auto hash = fc::sha256::hash( block_id );
    hash._hash[0] = nonce;
    return fc::sha256::hash( hash );
  }

  void pow_operation::validate()const
  {
#ifndef HIVE_CONVERTER_BUILD
    props.validate< true >();
    validate_account_name( worker_account );
    FC_ASSERT( work_input() == work.input, "Determninistic input does not match recorded input" );
    work.validate();
#endif /// HIVE_CONVERTER_BUILD
  }

  struct pow2_operation_validate_visitor
  {
    typedef void result_type;

    template< typename PowType >
    void operator()( const PowType& pow )const
    {
      pow.validate();
    }
  };

  void pow2_operation::validate()const
  {
#ifndef HIVE_CONVERTER_BUILD
    props.validate< true >();
    work.visit( pow2_operation_validate_visitor() );
#endif /// HIVE_CONVERTER_BUILD
  }

  struct pow2_operation_get_required_active_visitor
  {
    typedef void result_type;

    pow2_operation_get_required_active_visitor( flat_set< account_name_type >& required_active )
      : _required_active( required_active ) {}

    template< typename PowType >
    void operator()( const PowType& work )const
    {
      _required_active.insert( work.input.worker_account );
    }

    flat_set<account_name_type>& _required_active;
  };

  void pow2_operation::get_required_active_authorities( flat_set<account_name_type>& a )const
  {
    if( !new_owner_key )
    {
      pow2_operation_get_required_active_visitor vtor( a );
      work.visit( vtor );
    }
  }

  void pow::create( const fc::ecc::private_key& w, const digest_type& i )
  {
    input  = i;
    signature = w.sign_compact( input, fc::ecc::non_canonical );

    auto sig_hash            = fc::sha256::hash( signature );
    public_key_type recover  = fc::ecc::public_key( signature, sig_hash, fc::ecc::non_canonical );

    work = fc::sha256::hash(recover);
  }

  void pow2::create( const block_id_type& prev, const account_name_type& account_name, uint64_t n )
  {
    input.worker_account = account_name;
    input.prev_block     = prev;
    input.nonce          = n;

    auto prv_key = fc::sha256::hash( input );
    auto input = fc::sha256::hash( prv_key );
    auto signature = fc::ecc::private_key::regenerate( prv_key ).sign_compact( input, fc::ecc::fc_canonical );

    auto sig_hash            = fc::sha256::hash( signature );
    public_key_type recover  = fc::ecc::public_key( signature, sig_hash );

    fc::sha256 work = fc::sha256::hash(std::make_pair(input,recover));
    pow_summary = work.approx_log_32();
  }

  void equihash_pow::create( const block_id_type& recent_block, const account_name_type& account_name, uint32_t nonce )
  {
    input.worker_account = account_name;
    input.prev_block = recent_block;
    input.nonce = nonce;

    auto seed = fc::sha256::hash( input );
    proof = fc::equihash::proof::hash( HIVE_EQUIHASH_N, HIVE_EQUIHASH_K, seed );
    pow_summary = fc::sha256::hash( proof.inputs ).approx_log_32();
  }

  void pow::validate()const
  {
    FC_ASSERT( work != fc::sha256() );
    FC_ASSERT( public_key_type(fc::ecc::public_key( signature, input, fc::ecc::non_canonical ) ) == worker );
    auto sig_hash = fc::sha256::hash( signature );
    public_key_type recover  = fc::ecc::public_key( signature, sig_hash, fc::ecc::non_canonical );
    FC_ASSERT( work == fc::sha256::hash(recover) );
  }

  void pow2::validate()const
  {
    validate_account_name( input.worker_account );
    pow2 tmp; tmp.create( input.prev_block, input.worker_account, input.nonce );
    FC_ASSERT( pow_summary == tmp.pow_summary, "reported work does not match calculated work" );
  }

  void equihash_pow::validate() const
  {
    validate_account_name( input.worker_account );
    auto seed = fc::sha256::hash( input );
    FC_ASSERT( proof.n == HIVE_EQUIHASH_N, "proof of work 'n' value is incorrect" );
    FC_ASSERT( proof.k == HIVE_EQUIHASH_K, "proof of work 'k' value is incorrect" );
    FC_ASSERT( proof.seed == seed, "proof of work seed does not match expected seed" );
    FC_ASSERT( proof.is_valid(), "proof of work is not a solution", ("block_id", input.prev_block)("worker_account", input.worker_account)("nonce", input.nonce) );
    FC_ASSERT( pow_summary == fc::sha256::hash( proof.inputs ).approx_log_32() );
  }

  void feed_publish_operation::validate()const
  {
    validate_account_name( publisher );
    FC_ASSERT( ( is_asset_type( exchange_rate.base, HIVE_SYMBOL ) && is_asset_type( exchange_rate.quote, HBD_SYMBOL ) )
      || ( is_asset_type( exchange_rate.base, HBD_SYMBOL ) && is_asset_type( exchange_rate.quote, HIVE_SYMBOL ) ),
      "Price feed must be a HIVE/HBD price" );
    exchange_rate.validate();
  }

  void limit_order_create_operation::validate()const
  {
    validate_account_name( owner );

    FC_ASSERT(  ( is_asset_type( amount_to_sell, HIVE_SYMBOL ) && is_asset_type( min_to_receive, HBD_SYMBOL ) )
          || ( is_asset_type( amount_to_sell, HBD_SYMBOL ) && is_asset_type( min_to_receive, HIVE_SYMBOL ) )
          || (
              amount_to_sell.symbol.space() == asset_symbol_type::smt_nai_space
              && is_asset_type( min_to_receive, HIVE_SYMBOL )
            )
          || (
              is_asset_type( amount_to_sell, HIVE_SYMBOL )
              && min_to_receive.symbol.space() == asset_symbol_type::smt_nai_space
            ),
          "Limit order must be for the HIVE:HBD or SMT:(HIVE/HBD) market" );

    price( amount_to_sell, min_to_receive ).validate();
  }

  void limit_order_create2_operation::validate()const
  {
    validate_account_name( owner );

    FC_ASSERT( amount_to_sell.symbol == exchange_rate.base.symbol, "Sell asset must be the base of the price" );
    exchange_rate.validate();

    FC_ASSERT(  ( is_asset_type( amount_to_sell, HIVE_SYMBOL ) && is_asset_type( exchange_rate.quote, HBD_SYMBOL ) )
          || ( is_asset_type( amount_to_sell, HBD_SYMBOL ) && is_asset_type( exchange_rate.quote, HIVE_SYMBOL ) )
          || (
              amount_to_sell.symbol.space() == asset_symbol_type::smt_nai_space
              && is_asset_type( exchange_rate.quote, HIVE_SYMBOL )
            )
          || (
              is_asset_type( amount_to_sell, HIVE_SYMBOL )
              && exchange_rate.quote.symbol.space() == asset_symbol_type::smt_nai_space
            ),
          "Limit order must be for the HIVE:HBD or SMT:(HIVE/HBD) market" );

    FC_ASSERT( ( amount_to_sell * exchange_rate ).amount > 0, "Amount to sell cannot round to 0 when traded" );
  }

  void limit_order_cancel_operation::validate()const
  {
    validate_account_name( owner );
  }

  void convert_operation::validate()const
  {
    validate_account_name( owner );
    /// only allow conversion from HBD to HIVE, allowing the opposite can enable traders to abuse
    /// market fluxuations through converting large quantities without moving the price.
    FC_ASSERT( is_asset_type( amount, HBD_SYMBOL ), "Can only convert HBD to HIVE" );
    FC_ASSERT( amount.amount > 0, "Must convert some HBD" );
  }

  void collateralized_convert_operation::validate()const
  {
    validate_account_name( owner );
    /// only allow conversion from HIVE to HBD (at least for now)
    FC_ASSERT( is_asset_type( amount, HIVE_SYMBOL ), "Can only convert HIVE to HBD" );
    FC_ASSERT( amount.amount > 0, "Must convert some HIVE" );
  }

  void escrow_transfer_operation::validate()const
  {
    validate_account_name( from );
    validate_account_name( to );
    validate_account_name( agent );
    FC_ASSERT( fee.amount >= 0, "fee cannot be negative" );
    FC_ASSERT( hbd_amount.amount >= 0, "HBD amount cannot be negative" );
    FC_ASSERT( hive_amount.amount >= 0, "HIVE amount cannot be negative" );
    FC_ASSERT( hbd_amount.amount > 0 || hive_amount.amount > 0, "escrow must transfer a non-zero amount" );
    FC_ASSERT( from != agent && to != agent, "agent must be a third party" );
    FC_ASSERT( (fee.symbol == HIVE_SYMBOL) || (fee.symbol == HBD_SYMBOL), "fee must be HIVE or HBD" );
    FC_ASSERT( hbd_amount.symbol == HBD_SYMBOL, "HBD amount must contain HBD asset" );
    FC_ASSERT( hive_amount.symbol == HIVE_SYMBOL, "HIVE amount must contain HIVE asset" );
    FC_ASSERT( ratification_deadline < escrow_expiration, "ratification deadline must be before escrow expiration" );
    if (!json_meta.empty())
      validate_json_with_fallback(json_meta);
  }

  void escrow_approve_operation::validate()const
  {
    validate_account_name( from );
    validate_account_name( to );
    validate_account_name( agent );
    validate_account_name( who );
    FC_ASSERT( who == to || who == agent, "to or agent must approve escrow" );
  }

  void escrow_dispute_operation::validate()const
  {
    validate_account_name( from );
    validate_account_name( to );
    validate_account_name( agent );
    validate_account_name( who );
    FC_ASSERT( who == from || who == to, "who must be from or to" );
  }

  void escrow_release_operation::validate()const
  {
    validate_account_name( from );
    validate_account_name( to );
    validate_account_name( agent );
    validate_account_name( who );
    validate_account_name( receiver );
    FC_ASSERT( who == from || who == to || who == agent, "who must be from or to or agent" );
    FC_ASSERT( receiver == from || receiver == to, "receiver must be from or to" );
    FC_ASSERT( hbd_amount.amount >= 0, "HBD amount cannot be negative" );
    FC_ASSERT( hive_amount.amount >= 0, "HIVE amount cannot be negative" );
    FC_ASSERT( hbd_amount.amount > 0 || hive_amount.amount > 0, "escrow must release a non-zero amount" );
    FC_ASSERT( hbd_amount.symbol == HBD_SYMBOL, "HBD amount must contain HBD asset" );
    FC_ASSERT( hive_amount.symbol == HIVE_SYMBOL, "HIVE amount must contain HIVE asset" );
  }

  void request_account_recovery_operation::validate()const
  {
    validate_account_name( recovery_account );
    validate_account_name( account_to_recover );
    new_owner_authority.validate();
  }

  void recover_account_operation::validate()const
  {
    validate_account_name( account_to_recover );
    FC_ASSERT( !( new_owner_authority == recent_owner_authority ), "Cannot set new owner authority to the recent owner authority" );
    FC_ASSERT( !new_owner_authority.is_impossible(), "new owner authority cannot be impossible" );
    FC_ASSERT( !recent_owner_authority.is_impossible(), "recent owner authority cannot be impossible" );
    FC_ASSERT( new_owner_authority.weight_threshold, "new owner authority cannot be trivial" );
    new_owner_authority.validate();
    recent_owner_authority.validate();
  }

  void change_recovery_account_operation::validate()const
  {
    validate_account_name( account_to_recover );
    validate_account_name( new_recovery_account );
  }

  void transfer_to_savings_operation::validate()const {
    validate_account_name( from );
    validate_account_name( to );
    FC_ASSERT( amount.amount > 0 );
    FC_ASSERT( amount.symbol == HIVE_SYMBOL || amount.symbol == HBD_SYMBOL );
    FC_ASSERT( memo.size() < HIVE_MAX_MEMO_SIZE, "Memo is too large" );
    FC_ASSERT( fc::is_utf8( memo ), "Memo is not UTF8" );
  }
  void transfer_from_savings_operation::validate()const {
    validate_account_name( from );
    validate_account_name( to );
    FC_ASSERT( amount.amount > 0 );
    FC_ASSERT( amount.symbol == HIVE_SYMBOL || amount.symbol == HBD_SYMBOL );
    FC_ASSERT( memo.size() < HIVE_MAX_MEMO_SIZE, "Memo is too large" );
    FC_ASSERT( fc::is_utf8( memo ), "Memo is not UTF8" );
  }
  void cancel_transfer_from_savings_operation::validate()const {
    validate_account_name( from );
  }

  void decline_voting_rights_operation::validate()const
  {
    validate_account_name( account );
  }

  void reset_account_operation::validate()const
  {
    validate_account_name( reset_account );
    validate_account_name( account_to_reset );
    FC_ASSERT( !new_owner_authority.is_impossible(), "new owner authority cannot be impossible" );
    FC_ASSERT( new_owner_authority.weight_threshold, "new owner authority cannot be trivial" );
    new_owner_authority.validate();
  }

  void set_reset_account_operation::validate()const
  {
    validate_account_name( account );
    if( current_reset_account.size() )
      validate_account_name( current_reset_account );
    validate_account_name( reset_account );
    FC_ASSERT( current_reset_account != reset_account, "new reset account cannot be current reset account" );
  }

  void claim_reward_balance_operation::validate()const
  {
    validate_account_name( account );
    FC_ASSERT( is_asset_type( reward_hive, HIVE_SYMBOL ), "Reward HIVE must be expressed in HIVE" );
    FC_ASSERT( is_asset_type( reward_hbd, HBD_SYMBOL ), "Reward HBD must be expressed in HBD" );
    FC_ASSERT( is_asset_type( reward_vests, VESTS_SYMBOL ), "Reward VESTS must be expressed in VESTS" );
    FC_ASSERT( reward_hbd.amount >= 0, "Cannot claim a negative amount" );
    FC_ASSERT( reward_hive.amount >= 0, "Cannot claim a negative amount" );
    FC_ASSERT( reward_vests.amount >= 0, "Cannot claim a negative amount" );
    FC_ASSERT( reward_hive.amount > 0 || reward_hbd.amount > 0 || reward_vests.amount > 0, "Must claim something." );
  }

#ifdef HIVE_ENABLE_SMT
  void claim_reward_balance2_operation::validate()const
  {
    validate_account_name( account );
    FC_ASSERT( reward_tokens.empty() == false, "Must claim something." );
    FC_ASSERT( reward_tokens.begin()->amount >= 0, "Cannot claim a negative amount" );
    bool is_substantial_reward = reward_tokens.begin()->amount > 0;
    for( auto itl = reward_tokens.begin(), itr = itl+1; itr != reward_tokens.end(); ++itl, ++itr )
    {
      FC_ASSERT( itl->symbol.to_nai() <= itr->symbol.to_nai(),
              "Reward tokens have not been inserted in ascending order." );
      FC_ASSERT( itl->symbol.to_nai() != itr->symbol.to_nai(),
              "Duplicate symbol ${s} inserted into claim reward operation container.", ("s", itl->symbol) );
      FC_ASSERT( itr->amount >= 0, "Cannot claim a negative amount" );
      is_substantial_reward |= itr->amount > 0;
    }
    FC_ASSERT( is_substantial_reward, "Must claim something." );
  }
#endif

  void delegate_vesting_shares_operation::validate()const
  {
    validate_account_name( delegator );
    validate_account_name( delegatee );
    FC_ASSERT( delegator != delegatee, "You cannot delegate VESTS to yourself" );
    FC_ASSERT( is_asset_type( vesting_shares, VESTS_SYMBOL ), "Delegation must be VESTS" );
    FC_ASSERT( vesting_shares >= asset( 0, VESTS_SYMBOL ), "Delegation cannot be negative" );
  }

  void recurrent_transfer_operation::validate()const
  { try {
      validate_account_name( from );
      validate_account_name( to );
      FC_ASSERT( amount.symbol.is_vesting() == false, "Transfer of vesting is not allowed." );
      FC_ASSERT( amount.amount >= 0, "Cannot transfer a negative amount (aka: stealing)" );
      FC_ASSERT( recurrence >= HIVE_MIN_RECURRENT_TRANSFERS_RECURRENCE, "Cannot set a transfer recurrence that is less than ${recurrence} hours", ("recurrence", HIVE_MIN_RECURRENT_TRANSFERS_RECURRENCE) );
      FC_ASSERT( memo.size() < HIVE_MAX_MEMO_SIZE, "Memo is too large" );
      FC_ASSERT( fc::is_utf8( memo ), "Memo is not UTF8" );
      FC_ASSERT( from != to, "Cannot set a transfer to yourself" );
      FC_ASSERT(executions >= 2, "Executions must be at least 2, if you set executions to 1 the recurrent transfer will execute immediately and delete itself. You should use a normal transfer operation");
    } FC_CAPTURE_AND_RETHROW( (*this) )
  }

  void witness_block_approve_operation::validate()const
  { try {
      validate_account_name( witness );
    } FC_CAPTURE_AND_RETHROW( (*this) )
  }

} } // hive::protocol
