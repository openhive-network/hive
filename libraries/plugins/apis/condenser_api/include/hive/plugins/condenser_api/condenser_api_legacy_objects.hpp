#pragma once
#include <hive/chain/hive_fwd.hpp>
#include <hive/protocol/version.hpp>
#include <hive/protocol/version.hpp>
#include <hive/plugins/block_api/block_api_objects.hpp>

namespace hive { namespace plugins { namespace condenser_api {
using namespace hive::protocol;

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

FC_TODO( "Remove when automated actions are created" )
typedef static_variant<
      void_t,
      version,
      hardfork_version_vote
#ifdef IS_TEST_NET
,
      required_automated_actions,
      optional_automated_actions
#endif
    > legacy_block_header_extensions;

typedef vector< legacy_block_header_extensions > legacy_block_header_extensions_type;

struct legacy_signed_transaction
{
  using legacy_operation = serializer_wrapper<operation>;
  legacy_signed_transaction() {}

  legacy_signed_transaction( const signed_transaction& t ) :
    ref_block_num( t.ref_block_num ),
    ref_block_prefix( t.ref_block_prefix ),
    expiration( t.expiration )
  {
    // Signed transaction extensions field exists, but must be empty
    // Don't worry about copying them.
    operations.reserve(t.operations.size());
    for(const auto& v : t.operations)
      operations.emplace_back( legacy_operation{v} );

    signatures.insert( signatures.end(), t.signatures.begin(), t.signatures.end() );
  }

  legacy_signed_transaction( const annotated_signed_transaction& t ) :
    ref_block_num( t.ref_block_num ),
    ref_block_prefix( t.ref_block_prefix ),
    expiration( t.expiration ),
    transaction_id( t.transaction_id ),
    block_num( t.block_num ),
    transaction_num( t.transaction_num )
  {
    // Signed transaction extensions field exists, but must be empty
    // Don't worry about copying them.
    operations.reserve(t.operations.size());
    for(const auto& v : t.operations)
      operations.emplace_back( legacy_operation{v} );

    signatures.insert( signatures.end(), t.signatures.begin(), t.signatures.end() );
  }

  operator signed_transaction()const
  {
    signed_transaction tx;
    tx.ref_block_num = ref_block_num;
    tx.ref_block_prefix = ref_block_prefix;
    tx.expiration = expiration;

    tx.operations.reserve(operations.size());
    for(const auto& v : operations)
      tx.operations.emplace_back( v.value );
    tx.signatures.insert( tx.signatures.end(), signatures.begin(), signatures.end() );

    return tx;
  }

  uint16_t                   ref_block_num    = 0;
  uint32_t                   ref_block_prefix = 0;
  fc::time_point_sec         expiration;
  vector< legacy_operation > operations;
  extensions_type            extensions;
  vector< signature_type >   signatures;
  transaction_id_type        transaction_id;
  uint32_t                   block_num = 0;
  uint32_t                   transaction_num = 0;
};

struct legacy_signed_block
{
  legacy_signed_block() {}
  legacy_signed_block( const block_api::api_signed_block_object& b ) :
    previous( b.previous ),
    timestamp( b.timestamp ),
    witness( b.witness ),
    transaction_merkle_root( b.transaction_merkle_root ),
    witness_signature( b.witness_signature ),
    block_id( b.block_id ),
    signing_key( b.signing_key )
  {
    for( const auto& e : b.extensions )
    {
      legacy_block_header_extensions ext;
      e.visit( convert_to_legacy_static_variant< legacy_block_header_extensions >( ext ) );
      extensions.push_back( ext );
    }

    for( const auto& t : b.transactions )
    {
      transactions.push_back( legacy_signed_transaction( t ) );
    }

    transaction_ids.insert( transaction_ids.end(), b.transaction_ids.begin(), b.transaction_ids.end() );
  }

  operator signed_block()const
  {
    signed_block b;
    b.previous = previous;
    b.timestamp = timestamp;
    b.witness = witness;
    b.transaction_merkle_root = transaction_merkle_root;
    b.extensions.insert( extensions.begin(), extensions.end() );
    b.witness_signature = witness_signature;

    for( const auto& t : transactions )
    {
      b.transactions.push_back( signed_transaction( t ) );
    }

    return b;
  }

  block_id_type                       previous;
  fc::time_point_sec                  timestamp;
  string                              witness;
  checksum_type                       transaction_merkle_root;
  legacy_block_header_extensions_type extensions;
  signature_type                      witness_signature;
  vector< legacy_signed_transaction > transactions;
  block_id_type                       block_id;
  public_key_type                     signing_key;
  vector< transaction_id_type >       transaction_ids;
};

} } } // hive::plugins::condenser_api

FC_REFLECT( hive::plugins::condenser_api::legacy_price, (base)(quote) )

FC_REFLECT( hive::plugins::condenser_api::api_chain_properties,
        (account_creation_fee)(maximum_block_size)(hbd_interest_rate)(account_subsidy_budget)(account_subsidy_decay)
        )

FC_REFLECT( hive::plugins::condenser_api::legacy_signed_transaction,
        (ref_block_num)(ref_block_prefix)(expiration)(operations)(extensions)(signatures)(transaction_id)(block_num)(transaction_num) )

FC_REFLECT( hive::plugins::condenser_api::legacy_signed_block,
        (previous)(timestamp)(witness)(transaction_merkle_root)(extensions)(witness_signature)(transactions)(block_id)(signing_key)(transaction_ids) )
