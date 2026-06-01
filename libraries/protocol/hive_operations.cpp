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
        validate_is_utf8(json, "JSON not formatted in UTF8");
        HIVE_PROTOCOL_STRING_ASSERT(
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
    posting.validate();

    validate_auth_size( owner );
    validate_auth_size( active );
    validate_auth_size( posting );

    if (!json_metadata.empty())
      validate_json_with_fallback(json_metadata);

    validate_asset_not_negative(fee, "Account creation fee cannot be negative");
    HIVE_PROTOCOL_ASSET_ASSERT(
      fee <= asset( HIVE_MAX_ACCOUNT_CREATION_FEE, HIVE_SYMBOL ) && "account_create_operation",
      "Account creation fee cannot be too large",
      ("subject", fee)("max", HIVE_MAX_ACCOUNT_CREATION_FEE)
    );
  }

  void account_create_with_delegation_operation::validate() const
  {
    validate_account_name( new_account_name );
    validate_account_name( creator );
    validate_asset_type( fee, HIVE_SYMBOL, "Account creation fee must be HIVE" );
    validate_asset_type( delegation, VESTS_SYMBOL, "Delegation must be VESTS" );

    owner.validate();
    active.validate();
    posting.validate();

    validate_auth_size( owner );
    validate_auth_size( active );
    validate_auth_size( posting );

    if (!json_metadata.empty())
      validate_json_with_fallback(json_metadata);

    validate_asset_not_negative( fee, "Account creation fee cannot be negative");
    HIVE_PROTOCOL_ASSET_ASSERT(
      fee <= asset( HIVE_MAX_ACCOUNT_CREATION_FEE, HIVE_SYMBOL ) && "account_create_with_delegation_operation",
      "Account creation fee cannot be too large",
      ("subject", fee)("max", HIVE_MAX_ACCOUNT_CREATION_FEE)
    );
    validate_asset_not_negative( delegation, "Delegation cannot be negative");
  }

  void account_update_operation::validate() const
  {
    validate_account_name( account );

    // unfortunately can't have validate_auth_size here

    if (!json_metadata.empty())
      validate_json_with_fallback(json_metadata);
  }

  void account_update2_operation::validate() const
  {
    validate_account_name( account );

    if( owner )
      validate_auth_size( *owner );
    if( active )
      validate_auth_size( *active );
    if( posting )
      validate_auth_size( *posting );

    if (!json_metadata.empty())
      validate_json_with_fallback(json_metadata);

    if (!posting_json_metadata.empty())
      validate_json_with_fallback(posting_json_metadata);
  }

  void comment_operation::validate() const
  {
    validate_string_max_size(title, HIVE_COMMENT_TITLE_LIMIT - 1, "Title size limit exceeded.");
    validate_is_utf8(title, "Title not formatted in UTF8");
    validate_string_is_not_empty(body, "Body is empty");
    validate_is_utf8(body, "Body not formatted in UTF8");

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

    void operator()( const comment_payout_beneficiaries& cpb ) const
    {
      cpb.validate();
    }
  };

  void comment_payout_beneficiaries::validate()const
  {
    const char* exceed_100_percent_error_message = "Cannot allocate more than 100% of rewards to a comment";
    uint32_t sum = 0;

    HIVE_PROTOCOL_NUMBER_ASSERT( beneficiaries.size(), "Must specify at least one beneficiary", ("subject", beneficiaries.size()));
    HIVE_PROTOCOL_NUMBER_ASSERT( beneficiaries.size() < HIVE_BENEFICIARY_LIMIT,
      "Cannot specify more than ${max} beneficiaries.", ("max", HIVE_BENEFICIARY_LIMIT - 1)
      ("subject", beneficiaries.size())
    ); // Require size serialization fits in one byte.

    validate_account_name( beneficiaries[0].account );
    validate_number_in_100_percent_range(beneficiaries[0].weight, exceed_100_percent_error_message, *this);
    sum += beneficiaries[0].weight;
    validate_number_in_100_percent_range(sum, exceed_100_percent_error_message, *this);

    for( size_t i = 1; i < beneficiaries.size(); i++ )
    {
      validate_account_name( beneficiaries[i].account );
      validate_number_in_100_percent_range(beneficiaries[i].weight, exceed_100_percent_error_message, *this);

      sum += beneficiaries[i].weight;
      validate_number_in_100_percent_range(sum, exceed_100_percent_error_message, *this);
      HIVE_PROTOCOL_ACCOUNT_NAME_ASSERT( beneficiaries[i - 1] < beneficiaries[i], "Beneficiaries must be specified in sorted order (account ascending)", ("subject", beneficiaries[i].account)("previous", beneficiaries[i-1].account) );
    }
  }

  void comment_options_operation::validate()const
  {
    validate_account_name( author );
    validate_number_in_100_percent_range(percent_hbd, "Percent cannot exceed 100%");
    validate_asset_type(max_accepted_payout, HBD_SYMBOL, "Max accepted payout must be in HBD" );
    validate_asset_not_negative(max_accepted_payout, "Cannot accept less than 0 payout");
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
    validate_asset_type(fee, HIVE_SYMBOL, "Account claiming fee must be HIVE");
    validate_asset_not_negative(fee, "Account claiming fee cannot be negative");
    HIVE_PROTOCOL_ASSET_ASSERT(
      fee <= asset( HIVE_MAX_ACCOUNT_CREATION_FEE, HIVE_SYMBOL ),
      "Account claiming fee cannot be too large",
      ("subject", fee)("max", HIVE_MAX_ACCOUNT_CREATION_FEE)
    );

    HIVE_PROTOCOL_NUMBER_ASSERT(
      extensions.size() == 0 && "claim_account_operation",
      "There are no extensions for claim_account_operation.",
      ("subject", extensions.size())
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

    HIVE_PROTOCOL_NUMBER_ASSERT(
      extensions.size() == 0 && "create_claimed_account_operation",
      "There are no extensions for create_claimed_account_operation.",
      ("subject", extensions.size())
    );
  }

  void vote_operation::validate() const
  {
    validate_account_name( voter );
    validate_account_name( author );
    validate_number_in_100_percent_range(abs(weight), "Weight is not a HIVE percentage");
    validate_permlink( permlink );
  }

  void transfer_operation::validate() const
  { try {
    validate_account_name( from );
    validate_account_name( to );
    validate_asset_is_not_vesting(amount, "Transfer of vesting is not allowed.");
    validate_asset_greater_than_zero(amount, "Cannot transfer a negative amount (aka: stealing)");
    validate_string_max_size(memo, HIVE_MAX_MEMO_SIZE - 1, "Transfer memo is too large");
    validate_is_utf8(memo, "Transfer memo is not UTF8");
  } FC_CAPTURE_AND_RETHROW( (*this) ) }

  void transfer_to_vesting_operation::validate() const
  {
    validate_account_name( from );
    validate_asset_type( amount, HIVE_SYMBOL, "Amount must be HIVE" );
    if ( to != account_name_type() ) validate_account_name( to );
    validate_asset_greater_than_zero(amount, "Must transfer a nonzero amount");
  }

  void withdraw_vesting_operation::validate() const
  {
    validate_account_name( account );
    validate_asset_type(vesting_shares, VESTS_SYMBOL, "Amount must be VESTS");
  }

  void set_withdraw_vesting_route_operation::validate() const
  {
    validate_account_name( from_account );
    validate_account_name( to_account );
    HIVE_PROTOCOL_NUMBER_ASSERT(
      0 <= percent && percent <= HIVE_100_PERCENT,
      "Percent must be valid HIVE percent",
      ("subject", percent)
    );
  }

  void witness_update_operation::validate() const
  {
    validate_account_name( owner );

    validate_string_max_size(url, HIVE_MAX_WITNESS_URL_LENGTH, "URL is too long");
    validate_string_is_not_empty(url,  "URL size must be greater than 0");
    validate_is_utf8(url, "URL is not valid UTF8");
    validate_asset_not_negative(fee, "Fee cannot be negative");
    props.validate< false >();
  }

  void witness_set_properties_operation::validate() const
  {
    validate_account_name( owner );

    // current signing key must be present
    HIVE_PROTOCOL_STRING_ASSERT( props.find( "key" ) != props.end(), "No signing key provided", ("subject", "key") );

    auto itr = props.find( "account_creation_fee" );
    if( itr != props.end() )
    {
      asset account_creation_fee;
      fc::raw::unpack_from_vector( itr->second, account_creation_fee );
      validate_asset_type(account_creation_fee, HIVE_SYMBOL, "account_creation_fee must be in HIVE");
      HIVE_PROTOCOL_ASSET_ASSERT(
        account_creation_fee.amount >= HIVE_MIN_ACCOUNT_CREATION_FEE && "witness_set_properties_operation",
        "account_creation_fee smaller than minimum account creation fee",
        ("subject", account_creation_fee)(props) ("min", HIVE_MIN_ACCOUNT_CREATION_FEE)
      );
      HIVE_PROTOCOL_ASSET_ASSERT(
        account_creation_fee.amount <= HIVE_MAX_ACCOUNT_CREATION_FEE && "witness_set_properties_operation",
        "account_creation_fee greater than maximum account creation fee",
        ("subject", account_creation_fee)(props) ("max", HIVE_MAX_ACCOUNT_CREATION_FEE)
      );
    }

    itr = props.find( "maximum_block_size" );
    if( itr != props.end() )
    {
      uint32_t maximum_block_size = 0u;
      fc::raw::unpack_from_vector( itr->second, maximum_block_size );
      HIVE_PROTOCOL_NUMBER_ASSERT( maximum_block_size >= HIVE_MIN_BLOCK_SIZE_LIMIT && "witness_set_properties_operation",
        "maximum_block_size smaller than minimum max block size" ,
        ("subject", maximum_block_size)("min", HIVE_MIN_BLOCK_SIZE_LIMIT)
      );
      HIVE_PROTOCOL_NUMBER_ASSERT( maximum_block_size <= HIVE_MAX_BLOCK_SIZE && "witness_set_properties_operation",
        "Max block size cannot be more than 2MiB",
        ("subject", maximum_block_size)("max", HIVE_MAX_BLOCK_SIZE)
      );
    }

    itr = props.find( "sbd_interest_rate" );
    if( itr == props.end() )
      itr = props.find( "hbd_interest_rate" );

    if( itr != props.end() )
    {
      uint16_t hbd_interest_rate = 0u;
      fc::raw::unpack_from_vector( itr->second, hbd_interest_rate );
      HIVE_PROTOCOL_NUMBER_ASSERT(
        hbd_interest_rate >= 0 && "witness_set_properties_operation",
        "hbd_interest_rate must be positive",
        ("subject", hbd_interest_rate)
      );
      validate_number_in_100_percent_range(hbd_interest_rate, "hbd_interest_rate must not exceed 100%");
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
      validate_asset_type(hbd_exchange_rate.base, HBD_SYMBOL, "Base part of price feed must be in HBD");
      validate_asset_type(hbd_exchange_rate.quote, HIVE_SYMBOL, "Quote part of price feed must be in HIVE");
      hbd_exchange_rate.validate();
    }

    itr = props.find( "url" );
    if( itr != props.end() )
    {
      std::string url;
      fc::raw::unpack_from_vector< std::string >( itr->second, url );

      validate_string_max_size(url, HIVE_MAX_WITNESS_URL_LENGTH, "URL is too long");
      validate_string_is_not_empty(url, "URL size must be greater than 0");
      validate_is_utf8(url, "URL is not valid UTF8");
    }

    itr = props.find( "account_subsidy_budget" );
    if( itr != props.end() )
    {
      int32_t account_subsidy_budget  = 0u;
      fc::raw::unpack_from_vector( itr->second, account_subsidy_budget ); // Checks that the value can be deserialized
      HIVE_PROTOCOL_NUMBER_ASSERT( account_subsidy_budget >= HIVE_RD_MIN_BUDGET, "Budget must be at least ${n}", ("subject", account_subsidy_budget)("n", HIVE_RD_MIN_BUDGET) );
      HIVE_PROTOCOL_NUMBER_ASSERT( account_subsidy_budget <= HIVE_RD_MAX_BUDGET, "Budget must be at most ${n}", ("subject", account_subsidy_budget)("n", HIVE_RD_MAX_BUDGET) );
    }

    itr = props.find( "account_subsidy_decay" );
    if( itr != props.end() )
    {
      uint32_t account_subsidy_decay = 0u;
      fc::raw::unpack_from_vector( itr->second, account_subsidy_decay ); // Checks that the value can be deserialized
      HIVE_PROTOCOL_NUMBER_ASSERT( account_subsidy_decay >= HIVE_RD_MIN_DECAY, "Decay must be at least ${n}",
        ("subject", account_subsidy_decay)("n", HIVE_RD_MIN_DECAY)("min", HIVE_RD_MIN_DECAY)
      );
      HIVE_PROTOCOL_NUMBER_ASSERT( account_subsidy_decay <= HIVE_RD_MAX_DECAY, "Decay must be at most ${n}",
        ("subject", account_subsidy_decay)("n", HIVE_RD_MAX_DECAY)("max", HIVE_RD_MAX_DECAY)
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
    HIVE_PROTOCOL_ACCOUNT_NAME_ASSERT( proxy != account, "Cannot proxy to self", ("subject", proxy) );
  }

  void custom_operation::validate() const
  {
    /// required auth accounts are the ones whose bandwidth is consumed
    HIVE_PROTOCOL_NUMBER_ASSERT( required_auths.size() > 0, "at least one account must be specified", ("subject", required_auths.size()) );
    HIVE_PROTOCOL_NUMBER_ASSERT( required_auths.size() <= HIVE_MAX_AUTHORITY_MEMBERSHIP,
      "Authority membership exceeded. Max: ${max} Current: ${n}", ("max", HIVE_MAX_AUTHORITY_MEMBERSHIP)("n", required_auths.size())("subject", required_auths.size())
    );
  }

  void custom_json_operation::validate() const
  {
    /// required auth accounts are the ones whose bandwidth is consumed
    size_t num_auths = required_auths.size() + required_posting_auths.size();
    HIVE_PROTOCOL_NUMBER_ASSERT( num_auths > 0, "at least one account must be specified",
      ("subject", num_auths)("required_auths_size", required_auths.size())("required_posting_auths_size", required_posting_auths.size())
    );
    HIVE_PROTOCOL_NUMBER_ASSERT( num_auths <= HIVE_MAX_AUTHORITY_MEMBERSHIP,
      "Authority membership exceeded. Max: ${max} Current: ${n}", ("max", HIVE_MAX_AUTHORITY_MEMBERSHIP)("n", num_auths)("subject", num_auths)
    );
    HIVE_PROTOCOL_STRING_ASSERT( id.size() <= HIVE_CUSTOM_OP_ID_MAX_LENGTH,
      "Operation ID length exceeded. Max: ${max} Current: ${n}", ("subject", id)("max", HIVE_CUSTOM_OP_ID_MAX_LENGTH)("n", id.size()) );
    validate_json_with_fallback(json);
  }

  void custom_binary_operation::validate() const
  {
    size_t num_auths = required_owner_auths.size() + required_active_auths.size() + required_posting_auths.size();
    HIVE_PROTOCOL_NUMBER_ASSERT( num_auths > 0 && "binary", "at least one account must be specified",
      ("subject", num_auths)("num_auths", num_auths)
    ); // ABW: that limit has to hold at least until
      // https://gitlab.syncad.com/hive/hive/-/issues/675#note_159315 is implemented - someone has to pay RC
    for( const auto& auth : required_auths )
    {
      auth.validate();
      num_auths += auth.key_auths.size() + auth.account_auths.size();
    }
    HIVE_PROTOCOL_NUMBER_ASSERT( num_auths <= HIVE_MAX_AUTHORITY_MEMBERSHIP && "binary",
      "Authority membership exceeded. Max: ${max} Current: ${n}", ("max", HIVE_MAX_AUTHORITY_MEMBERSHIP)("n", num_auths)("subject", num_auths)
    );
    HIVE_PROTOCOL_STRING_ASSERT( HIVE_CUSTOM_OP_ID_MAX_LENGTH >= id.size(),
      "Operation ID length exceeded. Max: ${max} Current: ${n}", ("subject", id)("max", HIVE_CUSTOM_OP_ID_MAX_LENGTH)("n", id.size()) );
    HIVE_PROTOCOL_NUMBER_ASSERT( data.size() <= HIVE_CUSTOM_OP_DATA_MAX_LENGTH && "Too large",
      "Operation data must be less than ${bytes} bytes.", ("subject", data.size())("bytes", HIVE_CUSTOM_OP_DATA_MAX_LENGTH) );
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
    HIVE_PROTOCOL_STRING_ASSERT( winput == work.input, "Deterministic input does not match recorded input",
      ("subject", work.input)("expected", winput)("actual", work.input)
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
    HIVE_PROTOCOL_STRING_ASSERT( work != fc::sha256(), "work cannot consist of only zeroes", ("subject", work) );
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
    HIVE_PROTOCOL_NUMBER_ASSERT( proof.n == HIVE_EQUIHASH_N, "proof of work 'n' value is incorrect", ("subject", proof.n)("expected", HIVE_EQUIHASH_N) );
    HIVE_PROTOCOL_NUMBER_ASSERT( proof.k == HIVE_EQUIHASH_K, "proof of work 'k' value is incorrect", ("subject", proof.k)("expected", HIVE_EQUIHASH_K) );
    HIVE_PROTOCOL_STRING_ASSERT( proof.seed == seed, "proof of work seed does not match expected seed", ("subject", proof.seed)("expected", seed) );
    HIVE_PROTOCOL_STRING_ASSERT( proof.is_valid(), "proof of work is not a solution", ("subject", proof)("block_id", input.prev_block)("worker_account", input.worker_account)("nonce", input.nonce) );
    const auto calculated_summary = fc::sha256::hash( proof.inputs ).approx_log_32();
    HIVE_PROTOCOL_NUMBER_ASSERT( pow_summary == calculated_summary, "invalid proof summary", ("subject", pow_summary)("expected", calculated_summary) );
  }

  void feed_publish_operation::validate()const
  {
    validate_account_name( publisher );
    HIVE_PROTOCOL_ASSET_ASSERT( ( is_asset_type( exchange_rate.base, HIVE_SYMBOL ) && is_asset_type( exchange_rate.quote, HBD_SYMBOL ) )
      || ( is_asset_type( exchange_rate.base, HBD_SYMBOL ) && is_asset_type( exchange_rate.quote, HIVE_SYMBOL ) ),
      "Price feed must be a HIVE/HBD price", ("subject", exchange_rate) );
    exchange_rate.validate();
  }

  void limit_order_create_operation::validate()const
  {
    validate_account_name( owner );

    HIVE_PROTOCOL_ASSET_ASSERT(  ( is_asset_type( amount_to_sell, HIVE_SYMBOL ) && is_asset_type( min_to_receive, HBD_SYMBOL ) )
          || ( is_asset_type( amount_to_sell, HBD_SYMBOL ) && is_asset_type( min_to_receive, HIVE_SYMBOL ) )
          || (
              amount_to_sell.symbol.space() == asset_symbol_type::smt_nai_space
              && is_asset_type( min_to_receive, HIVE_SYMBOL )
            )
          || (
              is_asset_type( amount_to_sell, HIVE_SYMBOL )
              && min_to_receive.symbol.space() == asset_symbol_type::smt_nai_space
            ),
          "Limit order must be for the HIVE:HBD market", ("subject", amount_to_sell)(min_to_receive) );

    price( amount_to_sell, min_to_receive ).validate();
  }

  void limit_order_create2_operation::validate()const
  {
    validate_account_name( owner );
    validate_asset_type(amount_to_sell, exchange_rate.base.symbol, "Sell asset must be the base of the price");
    exchange_rate.validate();

    HIVE_PROTOCOL_ASSET_ASSERT(  ( is_asset_type( amount_to_sell, HIVE_SYMBOL ) && is_asset_type( exchange_rate.quote, HBD_SYMBOL ) )
          || ( is_asset_type( amount_to_sell, HBD_SYMBOL ) && is_asset_type( exchange_rate.quote, HIVE_SYMBOL ) )
          || (
              amount_to_sell.symbol.space() == asset_symbol_type::smt_nai_space
              && is_asset_type( exchange_rate.quote, HIVE_SYMBOL )
            )
          || (
              is_asset_type( amount_to_sell, HIVE_SYMBOL )
              && exchange_rate.quote.symbol.space() == asset_symbol_type::smt_nai_space
            ),
          "Limit order must be for the HIVE:HBD market", ("subject", amount_to_sell)(exchange_rate) );

    const auto amount_to_sell_in_quote = amount_to_sell * exchange_rate;
    validate_asset_greater_than_zero(amount_to_sell_in_quote, "Amount to sell cannot round to 0 when traded");
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
    validate_asset_type(amount, HBD_SYMBOL, "Can only convert HBD to HIVE");
    validate_asset_greater_than_zero(amount, "Must convert some HBD");
  }

  void collateralized_convert_operation::validate()const
  {
    validate_account_name( owner );
    /// only allow conversion from HIVE to HBD (at least for now)
    validate_asset_type(amount, HIVE_SYMBOL, "Can only convert HIVE to HBD");
    validate_asset_greater_than_zero(amount, "Must convert some HIVE");
  }

  void escrow_transfer_operation::validate()const
  {
    validate_account_name( from );
    validate_account_name( to );
    validate_account_name( agent );
    validate_asset_not_negative(fee, "fee cannot be negative");

    validate_asset_not_negative(hbd_amount, "hbd_amount cannot be negative");
    validate_asset_not_negative(hive_amount, "hive_amount cannot be negative");

    HIVE_PROTOCOL_ASSET_ASSERT( hbd_amount.amount > 0 || hive_amount.amount > 0, "escrow must transfer a non-zero amount", ("subject", hive_amount)(hbd_amount) );
    HIVE_PROTOCOL_ACCOUNT_NAME_ASSERT( from != agent && to != agent, "agent must be a third party", ("subject", agent) );

    validate_asset_type_one_of(fee, {HIVE_SYMBOL, HBD_SYMBOL}, "fee must be HIVE or HBD");
    validate_asset_type(hbd_amount, HBD_SYMBOL, "HBD amount must contain HBD asset");
    validate_asset_type(hive_amount, HIVE_SYMBOL, "HIVE amount must contain HIVE asset");

    HIVE_PROTOCOL_NUMBER_ASSERT( ratification_deadline < escrow_expiration, "ratification deadline must be before escrow expiration", ("subject", ratification_deadline)(escrow_expiration) );
    if (!json_meta.empty())
      validate_json_with_fallback(json_meta);
  }

  void escrow_approve_operation::validate()const
  {
    validate_account_name( from );
    validate_account_name( to );
    validate_account_name( agent );
    validate_account_name( who );
    HIVE_PROTOCOL_ACCOUNT_NAME_ASSERT( who == to || who == agent, "to or agent must approve escrow", ("subject", who) );
  }

  void escrow_dispute_operation::validate()const
  {
    validate_account_name( from );
    validate_account_name( to );
    validate_account_name( agent );
    validate_account_name( who );
    HIVE_PROTOCOL_ACCOUNT_NAME_ASSERT( who == from || who == to, "who must be from or to", ("subject", who) );
  }

  void escrow_release_operation::validate()const
  {
    validate_account_name( from );
    validate_account_name( to );
    validate_account_name( agent );
    validate_account_name( who );
    validate_account_name( receiver );
    HIVE_PROTOCOL_ACCOUNT_NAME_ASSERT( who == from || who == to || who == agent, "who must be from or to or agent", ("subject", who) );
    HIVE_PROTOCOL_ACCOUNT_NAME_ASSERT( receiver == from || receiver == to, "receiver must be from or to", ("subject", receiver) );
    validate_asset_not_negative(hbd_amount, "HBD amount cannot be negative");
    validate_asset_not_negative(hive_amount, "HIVE amount cannot be negative");
    HIVE_PROTOCOL_ASSET_ASSERT( (hbd_amount.amount > 0 || hive_amount.amount > 0) && "escrow_release_operation", "escrow must release a non-zero amount", ("subject", hive_amount)(hbd_amount) );
    validate_asset_type(hbd_amount, HBD_SYMBOL, "HBD amount must contain HBD asset");
    validate_asset_type(hive_amount, HIVE_SYMBOL, "HIVE amount must contain HIVE asset");
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
    HIVE_PROTOCOL_AUTHORITY_ASSERT( !( new_owner_authority == recent_owner_authority ), "Cannot set new owner authority to the recent owner authority", ("subject", new_owner_authority)("recent_owner_authority", recent_owner_authority) );
    HIVE_PROTOCOL_AUTHORITY_ASSERT( !new_owner_authority.is_impossible(), "new owner authority cannot be impossible", ("subject", new_owner_authority) );
    HIVE_PROTOCOL_AUTHORITY_ASSERT( !recent_owner_authority.is_impossible(), "recent owner authority cannot be impossible", ("subject", recent_owner_authority) );
    HIVE_PROTOCOL_AUTHORITY_ASSERT( new_owner_authority.weight_threshold, "new owner authority cannot be trivial", ("subject", new_owner_authority) );
    new_owner_authority.validate();
    recent_owner_authority.validate();
  }

  void change_recovery_account_operation::validate()const
  {
    validate_account_name( account_to_recover );
    validate_account_name( new_recovery_account );
  }

  void transfer_to_savings_operation::validate()const
  {
    validate_account_name( from );
    validate_account_name( to );
    validate_asset_greater_than_zero(amount, "Must transfer to savings some amount");
    validate_asset_type_one_of(amount, {HIVE_SYMBOL, HBD_SYMBOL}, "Must transfer HIVE or HBD");
    validate_string_max_size(memo, HIVE_MAX_MEMO_SIZE - 1,"Transfer to savings memo is too large");
    validate_is_utf8(memo, "Transfer to savings memo is not UTF8");
  }

  void transfer_from_savings_operation::validate()const
  {
    validate_account_name( from );
    validate_account_name( to );
    validate_asset_greater_than_zero(amount, "Must transfer from savings some amount");
    validate_asset_type_one_of(amount, {HIVE_SYMBOL, HBD_SYMBOL}, "Must transfer HIVE or HBD");
    validate_string_max_size(memo, HIVE_MAX_MEMO_SIZE - 1, "Transfer from savings memo is too large");
    validate_is_utf8(memo, "Transfer from savings memo is not UTF8");
  }

  void cancel_transfer_from_savings_operation::validate()const
  {
    validate_account_name( from );
  }

  void decline_voting_rights_operation::validate()const
  {
    validate_account_name( account );
  }

  void reset_account_operation::validate()const
  {
    FC_ASSERT( false && "placeholder1", "Placeholder for potential new operation" );
  }

  void set_reset_account_operation::validate()const
  {
    FC_ASSERT( false && "placeholder2", "Placeholder for potential new operation" );
  }

  void claim_reward_balance_operation::validate()const
  {
    validate_account_name( account );
    validate_asset_type(reward_hive, HIVE_SYMBOL, "Reward HIVE must be expressed in HIVE");
    validate_asset_type(reward_hbd, HBD_SYMBOL, "Reward HBD must be expressed in HBD");
    validate_asset_type(reward_vests, VESTS_SYMBOL, "Reward VESTS must be expressed in VESTS");
    validate_asset_not_negative(reward_hbd, "Cannot claim a negative amount");
    validate_asset_not_negative(reward_hive, "Cannot claim a negative amount");
    validate_asset_not_negative(reward_vests, "Cannot claim a negative amount");
    HIVE_PROTOCOL_ASSET_ASSERT( reward_hive.amount > 0 || reward_hbd.amount > 0 || reward_vests.amount > 0, "Must claim something.",
      ("subject", reward_hive)("min", 0)
    );
  }

  void delegate_vesting_shares_operation::validate()const
  {
    validate_account_name( delegator );
    validate_account_name( delegatee );
    HIVE_PROTOCOL_ACCOUNT_NAME_ASSERT( delegator != delegatee, "You cannot delegate VESTS to yourself", ("subject", delegatee)("account", delegator) );
    validate_asset_type( vesting_shares, VESTS_SYMBOL, "Delegation must be VESTS" );
    validate_asset_not_negative( vesting_shares, "Delegation cannot be negative" );
  }

  void recurrent_transfer_operation::validate()const
  { try {
      validate_account_name( from );
      validate_account_name( to );
      validate_asset_is_not_vesting(amount, "Recurrent transfer amount cannot be VESTS");
      validate_asset_not_negative(amount, "Recurrent transfer amount cannot be negative");
      HIVE_PROTOCOL_NUMBER_ASSERT( recurrence >= HIVE_MIN_RECURRENT_TRANSFERS_RECURRENCE, "Cannot set a transfer recurrence that is less than ${recurrence} hours", ("subject", recurrence)("recurrence", HIVE_MIN_RECURRENT_TRANSFERS_RECURRENCE) );
      validate_string_max_size(memo, HIVE_MAX_MEMO_SIZE - 1, "Recurrent transfer memo is too large");
      validate_is_utf8(memo, "Recurrent transfer memo is not UTF8");
      HIVE_PROTOCOL_ACCOUNT_NAME_ASSERT( from != to, "Cannot set a transfer to yourself", ("subject", to) );
      // The minimum number of executions (>= 2 when creating, and since HF29 >= 1 when modifying an
      // existing transfer) is enforced in the evaluator, which has access to hardfork state and knows
      // whether the operation creates or modifies a transfer. Here we only reject executions == 0.
      HIVE_PROTOCOL_NUMBER_ASSERT(
        executions >= 1,
        "Executions must be at least 1",
        ("subject", executions)("min", 1)
      );
    } FC_CAPTURE_AND_RETHROW( (*this) )
  }

  void witness_block_approve_operation::validate()const
  { try {
    validate_account_name( witness );
  } FC_CAPTURE_AND_RETHROW( (*this) ) }

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
