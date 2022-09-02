#pragma once
#include <hive/chain/hive_fwd.hpp>
#include <hive/plugins/condenser_api/condenser_api_legacy_operations.hpp>

#include <hive/plugins/account_history_api/annotated_signed_transaction.hpp>
#include <hive/plugins/block_api/block_api_objects.hpp>

namespace hive { namespace plugins { namespace condenser_api {

using hive::plugins::account_history::annotated_signed_transaction;

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
  legacy_signed_transaction() {}

  legacy_signed_transaction( const signed_transaction& t ) :
    ref_block_num( t.ref_block_num ),
    ref_block_prefix( t.ref_block_prefix ),
    expiration( t.expiration )
  {
    for( const auto& o : t.operations )
    {
      legacy_operation op;
      o.visit( legacy_operation_conversion_visitor( op ) );
      operations.push_back( op );
    }

    // Signed transaction extensions field exists, but must be empty
    // Don't worry about copying them.

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
    for( const auto& o : t.operations )
    {
      legacy_operation op;
      o.visit( legacy_operation_conversion_visitor( op ) );
      operations.push_back( op );
    }

    // Signed transaction extensions field exists, but must be empty
    // Don't worry about copying them.

    signatures.insert( signatures.end(), t.signatures.begin(), t.signatures.end() );
  }

  operator signed_transaction()const
  {
    signed_transaction tx;
    tx.ref_block_num = ref_block_num;
    tx.ref_block_prefix = ref_block_prefix;
    tx.expiration = expiration;

    convert_from_legacy_operation_visitor v;
    for( const auto& o : operations )
    {
      tx.operations.push_back( o.visit( v ) );
    }

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

    uint32_t block_num = b.block_num();
    uint32_t tx_no = 0;

    for( const auto& t : b.transactions )
    {
      legacy_signed_transaction& legcy_tx = transactions.emplace_back( legacy_signed_transaction( t ) );

      legcy_tx.transaction_id = b.transaction_ids[tx_no];
      legcy_tx.transaction_num = tx_no;
      legcy_tx.block_num = block_num;
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

FC_REFLECT( hive::plugins::condenser_api::legacy_signed_transaction,
        (ref_block_num)(ref_block_prefix)(expiration)(operations)(extensions)(signatures)(transaction_id)(block_num)(transaction_num) )

FC_REFLECT( hive::plugins::condenser_api::legacy_signed_block,
        (previous)(timestamp)(witness)(transaction_merkle_root)(extensions)(witness_signature)(transactions)(block_id)(signing_key)(transaction_ids) )
