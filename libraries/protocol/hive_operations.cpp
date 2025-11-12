#include <hive/protocol/config.hpp>
#include <hive/protocol/asset_symbol.hpp>
#include <hive/protocol/hive_operations.hpp>

#include <hive/protocol/validation.hpp>
#include <hive/protocol/hive_specialised_exceptions.hpp>

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
        validate_is_string_in_utf8(json, "JSON not formatted in UTF8");
        HIVE_PROTOCOL_VALIDATION_ASSERT(
          fc::json::is_valid(json, fc::json::format_validation_mode::relaxed /* json_validation_mode */), 
          "JSON is not valid JSON", 
          ("subject", json)
        );
      }
    }
  }

  void validate_auth_size( const authority& a )
  {
    size_t size = a.account_auths.size() + a.key_auths.size();
    HIVE_PROTOCOL_AUTHORITY_ASSERT( size <= HIVE_MAX_AUTHORITY_MEMBERSHIP,
      "Authority membership exceeded. Max: ${max} Current: ${n}", 
      ("max", HIVE_MAX_AUTHORITY_MEMBERSHIP)("n", size)("subject", a)
    );
  }

  void account_create_operation::validate() const
  {
    validate_account_name( new_account_name );
    validate_asset_type( fee, HIVE_SYMBOL, "Account creation fee must be HIVE" );
    owner.validate();
    active.validate();

    if (!json_metadata.empty())
      validate_json_with_fallback(json_metadata);

    validate_asset_not_negative(fee, "Account creation fee cannot be negative", *this);
  }

  void account_create_with_delegation_operation::validate() const
  {
    validate_account_name( new_account_name );
    validate_account_name( creator );
    validate_asset_type( fee, HIVE_SYMBOL, "Account creation fee must be HIVE", *this );
    validate_asset_type( delegation, VESTS_SYMBOL, "Delegation must be VESTS", *this);

    owner.validate();
    active.validate();
    posting.validate();

    if (!json_metadata.empty())
      validate_json_with_fallback(json_metadata);

    validate_asset_greater_than_zero( fee, "Account creation fee cannot be negative", *this);
    validate_asset_greater_than_zero( delegation, "Delegation cannot be negative", *this);
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
    validate_string_max_size(title, HIVE_COMMENT_TITLE_LIMIT, "Title size limit exceeded.", *this);
    validate_is_string_in_utf8(title, "Title not formatted in UTF8", *this);
    validate_string_is_not_empty(body, "Body is empty", *this);
    validate_is_string_in_utf8(body, "Body not formatted in UTF8", *this);


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
    const char* exceed_100_percent_error_message = "Cannot allocate more than 100% of rewards to a comment";
    uint32_t sum = 0;

    HIVE_PROTOCOL_OPERATIONS_ASSERT( beneficiaries.size(), "Must specify at least one beneficiary", ("subject", *this));
    HIVE_PROTOCOL_OPERATIONS_ASSERT( beneficiaries.size() < HIVE_BENEFICIARY_LIMIT,
      "Cannot specify more than ${max} beneficiaries.", ("max", HIVE_BENEFICIARY_LIMIT - 1)
      ("amount_of_beneficiaries", beneficiaries.size())("subject", *this)
    ); // Require size serialization fits in one byte.

    validate_account_name( beneficiaries[0].account );
    validate_number_in_100_percent_range(beneficiaries[0].weight, exceed_100_percent_error_message, *this);
    sum += beneficiaries[0].weight;
    validate_number_in_100_percent_range(sum, exceed_100_percent_error_message, *this);  // ??PROBLEM?? DOUBLE CHECK

    for( size_t i = 1; i < beneficiaries.size(); i++ )
    {
      validate_account_name( beneficiaries[i].account );
      validate_number_in_100_percent_range(beneficiaries[i].weight, exceed_100_percent_error_message, *this);

      sum += beneficiaries[i].weight;
      validate_number_in_100_percent_range(sum, exceed_100_percent_error_message, *this);
      HIVE_PROTOCOL_OPERATIONS_ASSERT( beneficiaries[i - 1] < beneficiaries[i], "Beneficiaries must be specified in sorted order (account ascending)" );
    }
  }

  void comment_options_operation::validate()const
  {
    validate_account_name( author );
    validate_number_in_100_percent_range(percent_hbd, "Percent cannot exceed 100%", *this);  // ??PROBLEM?? DOUBLE CHECK
    validate_asset_type(max_accepted_payout, HBD_SYMBOL, "Max accepted payout must be in HBD", *this );
    validate_asset_not_negative(max_accepted_payout, "Cannot accept less than 0 payout", *this);
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
    validate_asset_type(fee, HIVE_SYMBOL, "Account claiming fee must be HIVE", *this);
    validate_asset_greater_than_zero(fee, "Account claiming fee cannot be negative", *this);
    HIVE_PROTOCOL_ASSET_ASSERT( 
      fee <= asset( HIVE_MAX_ACCOUNT_CREATION_FEE, HIVE_SYMBOL ), 
      "Account claiming fee cannot be too large", 
      (fee)("max", HIVE_MAX_ACCOUNT_CREATION_FEE)("subject", *this) 
    );

    HIVE_PROTOCOL_OPERATIONS_ASSERT( 
      extensions.size() == 0 && "claim_account_operation", 
      "There are no extensions for claim_account_operation.",
      ("subject", *this)
    );
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

    HIVE_PROTOCOL_OPERATIONS_ASSERT( 
      extensions.size() == 0 && "create_claimed_account_operation", 
      "There are no extensions for create_claimed_account_operation.",
      ("subject", *this)
    );
  }

  void vote_operation::validate() const
  {
    validate_account_name( voter );
    validate_account_name( author );
    validate_number_in_100_percent_range(abs(weight), "Weight is not a HIVE percentage", *this);
    validate_permlink( permlink );
  }

  void transfer_operation::validate() const
  { try {
    validate_account_name( from );
    validate_account_name( to );
    validate_asset_is_not_vesting(amount, "Transfer of vesting is not allowed.", *this);
    validate_asset_greater_than_zero(amount, "Cannot transfer a negative amount (aka: stealing)", *this);
    validate_string_max_size(memo, HIVE_MAX_MEMO_SIZE, "Transfer memo is too large", *this);
    validate_is_string_in_utf8(memo, "Transfer memo is not UTF8", *this);
  } FC_CAPTURE_AND_RETHROW( (*this) ) }

  void transfer_to_vesting_operation::validate() const
  {
    validate_account_name( from );
    HIVE_PROTOCOL_OPERATIONS_ASSERT( amount.symbol == HIVE_SYMBOL ||
            ( amount.symbol.space() == asset_symbol_type::smt_nai_space && amount.symbol.is_vesting() == false ),
            "Amount must be HIVE or SMT liquid", (amount)("subject", *this) );
    if ( to != account_name_type() ) validate_account_name( to );
    validate_asset_greater_than_zero(amount, "Must transfer a nonzero amount", *this);
  }

  void withdraw_vesting_operation::validate() const
  {
    validate_account_name( account );
    validate_asset_type(vesting_shares, VESTS_SYMBOL, "Amount must be VESTS", *this);
  }

  void set_withdraw_vesting_route_operation::validate() const
  {
    validate_account_name( from_account );
    validate_account_name( to_account );
    HIVE_PROTOCOL_OPERATIONS_ASSERT( 
      0 <= percent && percent <= HIVE_100_PERCENT, 
      "Percent must be valid HIVE percent", 
      (percent)("subject", *this) 
    );
  }

  void witness_update_operation::validate() const
  {
    validate_account_name( owner );

    validate_string_max_size(url, HIVE_MAX_WITNESS_URL_LENGTH, "URL is too long", *this);
    validate_string_is_not_empty(url,  "URL size must be greater than 0", *this);
    validate_is_string_in_utf8(url, "URL is not valid UTF8", *this);
    validate_asset_not_negative(fee, "Fee cannot be negative", *this);
    props.validate< false >();
  }

  void witness_set_properties_operation::validate() const
  {
    validate_account_name( owner );

    // current signing key must be present
    HIVE_PROTOCOL_OPERATIONS_ASSERT( props.find( "key" ) != props.end(), "No signing key provided", ("subject", *this)(props) );

    auto itr = props.find( "account_creation_fee" );
    if( itr != props.end() )
    {
      asset account_creation_fee;
      fc::raw::unpack_from_vector( itr->second, account_creation_fee );
      validate_asset_type(account_creation_fee, HIVE_SYMBOL, "account_creation_fee must be in HIVE", *this);
      HIVE_PROTOCOL_OPERATIONS_ASSERT( 
        account_creation_fee.amount >= HIVE_MIN_ACCOUNT_CREATION_FEE && "witness_set_properties_operation", 
        "account_creation_fee smaller than minimum account creation fee", 
        ("subject", *this)(props) ("min", HIVE_MIN_ACCOUNT_CREATION_FEE)
      );
    }

    itr = props.find( "maximum_block_size" );
    if( itr != props.end() )
    {
      uint32_t maximum_block_size = 0u;
      fc::raw::unpack_from_vector( itr->second, maximum_block_size );
      HIVE_PROTOCOL_OPERATIONS_ASSERT( maximum_block_size >= HIVE_MIN_BLOCK_SIZE_LIMIT && "witness_set_properties_operation", 
        "maximum_block_size smaller than minimum max block size" ,
        ("subject", *this)(maximum_block_size)("min", HIVE_MIN_BLOCK_SIZE_LIMIT)
      );
    }

    itr = props.find( "sbd_interest_rate" );
    if( itr == props.end() )
      itr = props.find( "hbd_interest_rate" );

    if( itr != props.end() )
    {
      uint16_t hbd_interest_rate = 0u;
      fc::raw::unpack_from_vector( itr->second, hbd_interest_rate );
      HIVE_PROTOCOL_OPERATIONS_ASSERT( 
        hbd_interest_rate >= 0 && "witness_set_properties_operation", 
        "hbd_interest_rate must be positive",
        ("subject", *this)(hbd_interest_rate)
      );
      validate_number_in_100_percent_range(hbd_interest_rate, "hbd_interest_rate must not exceed 100%", *this);
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
      validate_asset_type(hbd_exchange_rate.base, HBD_SYMBOL, "Base part of price feed must be in HBD", *this);
      validate_asset_type(hbd_exchange_rate.quote, HIVE_SYMBOL, "Quote part of price feed must be in HIVE", *this);
      hbd_exchange_rate.validate();
    }

    itr = props.find( "url" );
    if( itr != props.end() )
    {
      std::string url;
      fc::raw::unpack_from_vector< std::string >( itr->second, url );

      validate_string_max_size(url, HIVE_MAX_WITNESS_URL_LENGTH, "URL is too long", *this);
      validate_string_is_not_empty(url, "URL size must be greater than 0", *this);
      validate_is_string_in_utf8(url, "URL is not valid UTF8", *this);
    }

    itr = props.find( "account_subsidy_budget" );
    if( itr != props.end() )
    {
      int32_t account_subsidy_budget  = 0u;
      fc::raw::unpack_from_vector( itr->second, account_subsidy_budget ); // Checks that the value can be deserialized
      HIVE_PROTOCOL_OPERATIONS_ASSERT( account_subsidy_budget >= HIVE_RD_MIN_BUDGET, "Budget must be at least ${n}", ("n", HIVE_RD_MIN_BUDGET)("subject", *this) );
      HIVE_PROTOCOL_OPERATIONS_ASSERT( account_subsidy_budget <= HIVE_RD_MAX_BUDGET, "Budget must be at most ${n}", ("n", HIVE_RD_MAX_BUDGET)("subject", *this) );
    }

    itr = props.find( "account_subsidy_decay" );
    if( itr != props.end() )
    {
      uint32_t account_subsidy_decay = 0u;
      fc::raw::unpack_from_vector( itr->second, account_subsidy_decay ); // Checks that the value can be deserialized
      HIVE_PROTOCOL_OPERATIONS_ASSERT( account_subsidy_decay >= HIVE_RD_MIN_DECAY, "Decay must be at least ${n}", 
        ("n", HIVE_RD_MIN_DECAY)("min", HIVE_RD_MIN_DECAY)("subject", *this) 
      );
      HIVE_PROTOCOL_OPERATIONS_ASSERT( account_subsidy_decay <= HIVE_RD_MAX_DECAY, "Decay must be at most ${n}", 
        ("n", HIVE_RD_MAX_DECAY)("max", HIVE_RD_MAX_DECAY)("subject", *this) 
      );
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
    HIVE_PROTOCOL_OPERATIONS_ASSERT( proxy != account, "Cannot proxy to self", ("subject", *this)(account) );
  }

  void custom_operation::validate() const {
    /// required auth accounts are the ones whose bandwidth is consumed
    HIVE_PROTOCOL_OPERATIONS_ASSERT( required_auths.size() > 0, "at least one account must be specified", (required_auths)("subject", *this) );
  }
  void custom_json_operation::validate() const {
    /// required auth accounts are the ones whose bandwidth is consumed
    HIVE_PROTOCOL_AUTHORITY_ASSERT( (required_auths.size() + required_posting_auths.size()) > 0, "at least one account must be specified", 
      ("subject", *this)("required_auths_size", required_auths.size())("required_posting_auths_size", required_posting_auths.size())
    );
    HIVE_PROTOCOL_OPERATIONS_ASSERT( id.size() <= HIVE_CUSTOM_OP_ID_MAX_LENGTH,
      "Operation ID length exceeded. Max: ${max} Current: ${n}", ("max", HIVE_CUSTOM_OP_ID_MAX_LENGTH)("n", id.size())("subject", *this) );
    validate_json_with_fallback(json);
  }
  void custom_binary_operation::validate() const {
    /// required auth accounts are the ones whose bandwidth is consumed
    const int total_required_auths = required_auths.size() + required_posting_auths.size() + required_owner_auths.size();
    HIVE_PROTOCOL_AUTHORITY_ASSERT( total_required_auths > 0, "at least one account must be specified", ("subject", *this)("total_required_auths", total_required_auths));
    HIVE_PROTOCOL_OPERATIONS_ASSERT( HIVE_CUSTOM_OP_ID_MAX_LENGTH >= id.size(),
      "Operation ID length exceeded. Max: ${max} Current: ${n}", ("max", HIVE_CUSTOM_OP_ID_MAX_LENGTH)("n", id.size())("subject", *this) );
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
    const auto winput = work_input();
    HIVE_PROTOCOL_OPERATIONS_ASSERT( winput == work.input, "Determninistic input does not match recorded input", 
      ("subject", *this)("expected", winput)("actual", work.input) 
    );
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

  void pow::create(const fc::ecc::private_key& w, const digest_type& i)
  {
    input = i;
    signature = w.sign_compact(input);

    auto sig_hash = fc::sha256::hash(signature);
    public_key_type recover = fc::ecc::public_key(signature, sig_hash);

    work = fc::sha256::hash(recover);
  }

void pow2::create(const block_id_type & prev, const account_name_type & account_name, uint64_t n)
  {
  input.worker_account = account_name;
  input.prev_block = prev;
  input.nonce = n;

  auto prv_key = fc::sha256::hash(input);
  auto input = fc::sha256::hash(prv_key);
  auto signature = fc::ecc::private_key::regenerate(prv_key).sign_compact(input);

  auto sig_hash = fc::sha256::hash(signature);
  public_key_type recover = fc::ecc::public_key(signature, sig_hash);

  fc::sha256 work = fc::sha256::hash(std::make_pair(input, recover));
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
    HIVE_PROTOCOL_OPERATIONS_ASSERT( work != fc::sha256(), "work cannot consist of only zeroes", ("subject", *this)(work) );
    /// Code eliminated due to hard switch to only bip0062 working mode - all such operations are VALID since they are already in blocks. Creation of new ops is disallowed.
    //HIVE_PROTOCOL_VALIDATION_ASSERT( public_key_type(fc::ecc::public_key( signature, input, fc::ecc::non_canonical) ) == worker );
    //auto sig_hash = fc::sha256::hash( signature );
    //public_key_type recover  = fc::ecc::public_key( signature, sig_hash, fc::ecc::non_canonical);
    //HIVE_PROTOCOL_OPERATIONS_ASSERT( work == fc::sha256::hash(recover) );
  }

  void pow2::validate()const
  {
    validate_account_name( input.worker_account );
    /// Code eliminated due to hard switch to only bip0062 working mode - all such operations are VALID since they are already in blocks. Creation of new ops is disallowed.
    //pow2 tmp; tmp.create( input.prev_block, input.worker_account, input.nonce );
    //HIVE_PROTOCOL_OPERATIONS_ASSERT( pow_summary == tmp.pow_summary, "reported work does not match calculated work" );
  }

  void equihash_pow::validate() const
  {
    validate_account_name( input.worker_account );
    auto seed = fc::sha256::hash( input );
    HIVE_PROTOCOL_OPERATIONS_ASSERT( proof.n == HIVE_EQUIHASH_N, "proof of work 'n' value is incorrect", ("proof_n", proof.n)("expected", HIVE_EQUIHASH_N)("subject", *this) );
    HIVE_PROTOCOL_OPERATIONS_ASSERT( proof.k == HIVE_EQUIHASH_K, "proof of work 'k' value is incorrect", ("proof_k", proof.k)("expected", HIVE_EQUIHASH_K)("subject", *this) );
    HIVE_PROTOCOL_OPERATIONS_ASSERT( proof.seed == seed, "proof of work seed does not match expected seed", ("proof_seed", proof.seed)("expected", seed)("subject", *this) );
    HIVE_PROTOCOL_OPERATIONS_ASSERT( proof.is_valid(), "proof of work is not a solution", ("block_id", input.prev_block)("worker_account", input.worker_account)("nonce", input.nonce)("subject", *this) );
    const auto calculated_summary = fc::sha256::hash( proof.inputs ).approx_log_32();
    HIVE_PROTOCOL_OPERATIONS_ASSERT( pow_summary == calculated_summary, "invalid proof summary", ("expected", calculated_summary)("actual", pow_summary)("subject", *this) );
  }

  void feed_publish_operation::validate()const
  {
    validate_account_name( publisher );
    HIVE_PROTOCOL_ASSET_ASSERT( ( is_asset_type( exchange_rate.base, HIVE_SYMBOL ) && is_asset_type( exchange_rate.quote, HBD_SYMBOL ) )
      || ( is_asset_type( exchange_rate.base, HBD_SYMBOL ) && is_asset_type( exchange_rate.quote, HIVE_SYMBOL ) ),
      "Price feed must be a HIVE/HBD price", (exchange_rate)("subject", *this) );
    exchange_rate.validate();
  }

  void limit_order_create_operation::validate()const
  {
    validate_account_name( owner );

    HIVE_PROTOCOL_OPERATIONS_ASSERT(  ( is_asset_type( amount_to_sell, HIVE_SYMBOL ) && is_asset_type( min_to_receive, HBD_SYMBOL ) )
          || ( is_asset_type( amount_to_sell, HBD_SYMBOL ) && is_asset_type( min_to_receive, HIVE_SYMBOL ) )
          || (
              amount_to_sell.symbol.space() == asset_symbol_type::smt_nai_space
              && is_asset_type( min_to_receive, HIVE_SYMBOL )
            )
          || (
              is_asset_type( amount_to_sell, HIVE_SYMBOL )
              && min_to_receive.symbol.space() == asset_symbol_type::smt_nai_space
            ),
          "Limit order must be for the HIVE:HBD or SMT:(HIVE/HBD) market", (amount_to_sell)(min_to_receive)("subject", *this) );

    price( amount_to_sell, min_to_receive ).validate();
  }

  void limit_order_create2_operation::validate()const
  {
    validate_account_name( owner );
    validate_asset_type(amount_to_sell, exchange_rate.base.symbol, "Sell asset must be the base of the price", *this);
    exchange_rate.validate();

    HIVE_PROTOCOL_OPERATIONS_ASSERT(  ( is_asset_type( amount_to_sell, HIVE_SYMBOL ) && is_asset_type( exchange_rate.quote, HBD_SYMBOL ) )
          || ( is_asset_type( amount_to_sell, HBD_SYMBOL ) && is_asset_type( exchange_rate.quote, HIVE_SYMBOL ) )
          || (
              amount_to_sell.symbol.space() == asset_symbol_type::smt_nai_space
              && is_asset_type( exchange_rate.quote, HIVE_SYMBOL )
            )
          || (
              is_asset_type( amount_to_sell, HIVE_SYMBOL )
              && exchange_rate.quote.symbol.space() == asset_symbol_type::smt_nai_space
            ),
          "Limit order must be for the HIVE:HBD or SMT:(HIVE/HBD) market", (amount_to_sell)(exchange_rate)("subject", *this) );

    const auto amount_to_sell_in_quote = amount_to_sell * exchange_rate;
    validate_asset_greater_than_zero(amount_to_sell_in_quote, "Amount to sell cannot round to 0 when traded", *this);
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
    validate_asset_type(amount, HBD_SYMBOL, "Can only convert HBD to HIVE", *this);
    validate_asset_greater_than_zero(amount, "Must convert some HBD", *this);
  }

  void collateralized_convert_operation::validate()const
  {
    validate_account_name( owner );
    /// only allow conversion from HIVE to HBD (at least for now)
    validate_asset_type(amount, HIVE_SYMBOL, "Can only convert HIVE to HBD", *this);
    validate_asset_greater_than_zero(amount, "Must convert some HIVE", *this);
  }

  void escrow_transfer_operation::validate()const
  {
    validate_account_name( from );
    validate_account_name( to );
    validate_account_name( agent );
    validate_asset_greater_than_zero(fee, "fee cannot be negative", *this);
    
    validate_asset_not_negative(hbd_amount, "hbd_amount cannot be negative", *this);
    validate_asset_not_negative(hive_amount, "hive_amount cannot be negative", *this);
    
    HIVE_PROTOCOL_OPERATIONS_ASSERT( hbd_amount.amount > 0 || hive_amount.amount > 0, "escrow must transfer a non-zero amount", ("subject", *this) );
    HIVE_PROTOCOL_OPERATIONS_ASSERT( from != agent && to != agent, "agent must be a third party", ("subject", *this) );

    validate_asset_type_one_of(fee, {HIVE_SYMBOL, HBD_SYMBOL}, "fee must be HIVE or HBD", *this);
    validate_asset_type(hbd_amount, HBD_SYMBOL, "HBD amount must contain HBD asset", *this);
    validate_asset_type(hive_amount, HIVE_SYMBOL, "HIVE amount must contain HIVE asset", *this);

    HIVE_PROTOCOL_OPERATIONS_ASSERT( ratification_deadline < escrow_expiration, "ratification deadline must be before escrow expiration", ("subject", *this) );
    if (!json_meta.empty())
      validate_json_with_fallback(json_meta);
  }

  void escrow_approve_operation::validate()const
  {
    validate_account_name( from );
    validate_account_name( to );
    validate_account_name( agent );
    validate_account_name( who );
    HIVE_PROTOCOL_OPERATIONS_ASSERT( who == to || who == agent, "to or agent must approve escrow", ("subject", *this) );
  }

  void escrow_dispute_operation::validate()const
  {
    validate_account_name( from );
    validate_account_name( to );
    validate_account_name( agent );
    validate_account_name( who );
    HIVE_PROTOCOL_OPERATIONS_ASSERT( who == from || who == to, "who must be from or to", ("subject", *this) );
  }

  void escrow_release_operation::validate()const
  {
    validate_account_name( from );
    validate_account_name( to );
    validate_account_name( agent );
    validate_account_name( who );
    validate_account_name( receiver );
    HIVE_PROTOCOL_OPERATIONS_ASSERT( who == from || who == to || who == agent, "who must be from or to or agent", ("subject", *this) );
    HIVE_PROTOCOL_OPERATIONS_ASSERT( receiver == from || receiver == to, "receiver must be from or to", ("subject", *this) );
    validate_asset_not_negative(hbd_amount, "HBD amount cannot be negative", *this);
    validate_asset_not_negative(hive_amount, "HIVE amount cannot be negative", *this);
    HIVE_PROTOCOL_OPERATIONS_ASSERT( (hbd_amount.amount > 0 || hive_amount.amount > 0) && "escrow_release_operation", "escrow must release a non-zero amount", ("subject", *this) );
    validate_asset_type(hbd_amount, HBD_SYMBOL, "HBD amount must contain HBD asset", *this);
    validate_asset_type(hive_amount, HIVE_SYMBOL, "HIVE amount must contain HIVE asset", *this);
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
    HIVE_PROTOCOL_AUTHORITY_ASSERT( !( new_owner_authority == recent_owner_authority ), "Cannot set new owner authority to the recent owner authority", ("subject", *this) );
    HIVE_PROTOCOL_AUTHORITY_ASSERT( !new_owner_authority.is_impossible(), "new owner authority cannot be impossible", ("subject", *this) );
    HIVE_PROTOCOL_AUTHORITY_ASSERT( !recent_owner_authority.is_impossible(), "recent owner authority cannot be impossible", ("subject", *this) );
    HIVE_PROTOCOL_AUTHORITY_ASSERT( new_owner_authority.weight_threshold, "new owner authority cannot be trivial", ("subject", *this) );
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
    validate_asset_greater_than_zero(amount, "Must transfer to savings some amount", *this);
    validate_asset_type_one_of(amount, {HIVE_SYMBOL, HBD_SYMBOL}, "Must transfer HIVE or HBD", *this);
    validate_string_max_size(memo, HIVE_MAX_MEMO_SIZE,"Transfer to savings memo is too large", *this);
    validate_is_string_in_utf8(memo, "Transfer to savings memo is not UTF8", *this);
  }
  void transfer_from_savings_operation::validate()const {
    validate_account_name( from );
    validate_account_name( to );
    validate_asset_greater_than_zero(amount, "Must transfer from savings some amount", *this);
    validate_asset_type_one_of(amount, {HIVE_SYMBOL, HBD_SYMBOL}, "Must transfer HIVE or HBD", *this);
    validate_string_max_size(memo, HIVE_MAX_MEMO_SIZE, "Transfer from savings memo is too large", *this);
    validate_is_string_in_utf8(memo, "Transfer from savings memo is not UTF8", *this);
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
    HIVE_PROTOCOL_AUTHORITY_ASSERT( not new_owner_authority.is_impossible(), "new owner authority cannot be impossible", ("subject", *this) );
    HIVE_PROTOCOL_AUTHORITY_ASSERT( new_owner_authority.weight_threshold > 0, "new owner authority cannot be trivial", ("subject", *this) );
    new_owner_authority.validate();
  }

  void set_reset_account_operation::validate()const
  {
    validate_account_name( account );
    if( current_reset_account.size() )
      validate_account_name( current_reset_account );
    validate_account_name( reset_account );
    HIVE_PROTOCOL_OPERATIONS_ASSERT( current_reset_account != reset_account, "new reset account cannot be current reset account" );
  }

  void claim_reward_balance_operation::validate()const
  {
    validate_account_name( account );
    validate_asset_type(reward_hive, HIVE_SYMBOL, "Reward HIVE must be expressed in HIVE", *this);
    validate_asset_type(reward_hbd, HBD_SYMBOL, "Reward HBD must be expressed in HBD", *this);
    validate_asset_type(reward_vests, VESTS_SYMBOL, "Reward VESTS must be expressed in VESTS", *this);
    validate_asset_not_negative(reward_hbd, "Cannot claim a negative amount", *this);
    validate_asset_not_negative(reward_hive, "Cannot claim a negative amount", *this);
    validate_asset_not_negative(reward_vests, "Cannot claim a negative amount", *this);
    HIVE_PROTOCOL_OPERATIONS_ASSERT( reward_hive.amount > 0 || reward_hbd.amount > 0 || reward_vests.amount > 0, "Must claim something.", 
      ("subject", *this)
      ("min", 0) 
    );
  }

#ifdef HIVE_ENABLE_SMT
  void claim_reward_balance2_operation::validate()const
  {
    validate_account_name( account );
    HIVE_PROTOCOL_OPERATIONS_ASSERT( reward_tokens.empty() == false, "Must claim something." );
    HIVE_PROTOCOL_OPERATIONS_ASSERT( reward_tokens.begin()->amount >= 0, "Cannot claim a negative amount" );
    bool is_substantial_reward = reward_tokens.begin()->amount > 0;
    for( auto itl = reward_tokens.begin(), itr = itl+1; itr != reward_tokens.end(); ++itl, ++itr )
    {
      HIVE_PROTOCOL_ASSET_ASSERT( itl->symbol.to_nai() <= itr->symbol.to_nai(),
              "Reward tokens have not been inserted in ascending order.");
      HIVE_PROTOCOL_ASSET_ASSERT( itl->symbol.to_nai() != itr->symbol.to_nai(),
              "Duplicate symbol ${s} inserted into claim reward operation container.", ("s", itl->symbol) );
      HIVE_PROTOCOL_OPERATIONS_ASSERT( itr->amount >= 0, "Cannot claim a negative amount" );
      is_substantial_reward |= itr->amount > 0;
    }
    HIVE_PROTOCOL_OPERATIONS_ASSERT( is_substantial_reward, "Must claim something." );
  }
#endif

  void delegate_vesting_shares_operation::validate()const
  {
    validate_account_name( delegator );
    validate_account_name( delegatee );
    HIVE_PROTOCOL_OPERATIONS_ASSERT( delegator != delegatee, "You cannot delegate VESTS to yourself", ("subject", *this)("account", delegator) );
    validate_asset_type( vesting_shares, VESTS_SYMBOL, "Delegation must be VESTS", *this );
    validate_asset_not_negative( vesting_shares, "Delegation cannot be negative", *this );
  }

  void recurrent_transfer_operation::validate()const
  { try {
      const size_t minimum_amount_of_executions{2};

      validate_account_name( from );
      validate_account_name( to );
      validate_asset_is_not_vesting(amount, "Recurrent transfer amount cannot be VESTS", *this);
      validate_asset_not_negative(amount, "Recurrent transfer amount cannot be negative", *this);
      HIVE_PROTOCOL_OPERATIONS_ASSERT( recurrence >= HIVE_MIN_RECURRENT_TRANSFERS_RECURRENCE, "Cannot set a transfer recurrence that is less than ${recurrence} hours", ("recurrence", HIVE_MIN_RECURRENT_TRANSFERS_RECURRENCE) );
      validate_string_max_size(memo, HIVE_MAX_MEMO_SIZE, "Recurrent transfer memo is too large", *this);
      validate_is_string_in_utf8(memo, "Recurrent transfer memo is not UTF8", *this);
      HIVE_PROTOCOL_OPERATIONS_ASSERT( from != to, "Cannot set a transfer to yourself" );
      HIVE_PROTOCOL_OPERATIONS_ASSERT(
        executions >= minimum_amount_of_executions, 
        "Executions must be at least 2, if you set executions to 1 the recurrent transfer will execute immediately and delete itself. You should use a normal transfer operation",
        ("subject", *this)(executions)("min", minimum_amount_of_executions)
      );
    } FC_CAPTURE_AND_RETHROW( (*this) )
  }

  void witness_block_approve_operation::validate()const
  { try {
      validate_account_name( witness );
    } FC_CAPTURE_AND_RETHROW( (*this) )
  }

  struct recurrent_transfer_extension_visitor
  {
    uint8_t pair_id = 0; // default recurrent transfer id is 0
    bool was_pair_id = false;

    typedef void result_type;

    void operator()( const recurrent_transfer_pair_id& recurrent_transfer_pair_id )
    {
      was_pair_id = true;
      pair_id = recurrent_transfer_pair_id.pair_id;
    }

    void operator()( const hive::void_t& ) {}
  };

  uint8_t recurrent_transfer_operation::get_pair_id( const recurrent_transfer_extensions_type& _extensions, bool* explicitValue ) const
  {
    recurrent_transfer_extension_visitor _vtor;

    for( const auto& e : _extensions )
      e.visit( _vtor );

    if( explicitValue )
      *explicitValue = _vtor.was_pair_id;

    return _vtor.pair_id;
  }

  uint8_t recurrent_transfer_operation::get_pair_id( bool* explicitValue ) const
  {
    return get_pair_id( extensions, explicitValue );
  }

} } // hive::protocol
