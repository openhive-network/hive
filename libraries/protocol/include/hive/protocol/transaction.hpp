#pragma once
#include <hive/protocol/operations.hpp>
#include <hive/protocol/sign_state.hpp>
#include <hive/protocol/types.hpp>

#include <functional>
#include <numeric>

namespace hive { namespace protocol {

using fc::ecc::canonical_signature_type;

  struct transaction
  {
    uint16_t           ref_block_num    = 0;
    uint32_t           ref_block_prefix = 0;

    fc::time_point_sec expiration;

    vector<operation>  operations;
    extensions_type    extensions;

    digest_type         digest()const;
    transaction_id_type id()const;
    void                validate() const;
    void                validate( const std::function<void( const operation& op, bool post )>& notify ) const;
    digest_type         sig_digest( const chain_id_type& chain_id, hive::protocol::pack_type pack )const;

    void set_expiration( fc::time_point_sec expiration_time );
    void set_reference_block( const block_id_type& reference_block );

    template<typename Visitor>
    vector<typename Visitor::result_type> visit( Visitor&& visitor )
    {
      vector<typename Visitor::result_type> results;
      for( auto& op : operations )
        results.push_back(op.visit( std::forward<Visitor>( visitor ) ));
      return results;
    }
    template<typename Visitor>
    vector<typename Visitor::result_type> visit( Visitor&& visitor )const
    {
      vector<typename Visitor::result_type> results;
      for( auto& op : operations )
        results.push_back(op.visit( std::forward<Visitor>( visitor ) ));
      return results;
    }

    void get_required_authorities( flat_set< account_name_type >& active,
                          flat_set< account_name_type >& owner,
                          flat_set< account_name_type >& posting,
                          flat_set< account_name_type >& witness,
                          vector< authority >& other )const;
  };

  struct signed_transaction : public transaction
  {
    signed_transaction( const transaction& trx = transaction() )
      : transaction(trx){}

    //const signature_type& sign( const private_key_type& key, const chain_id_type& chain_id, canonical_signature_type canon_type/* = fc::ecc::fc_canonical*/ );

    //signature_type sign( const private_key_type& key, const chain_id_type& chain_id, canonical_signature_type canon_type/* = fc::ecc::fc_canonical*/ )const;

    set<public_key_type> get_required_signatures(
      const chain_id_type& chain_id,
      const flat_set<public_key_type>& available_keys,
      const authority_getter& get_active,
      const authority_getter& get_owner,
      const authority_getter& get_posting,
      const witness_public_key_getter& get_witness_key,
      uint32_t max_recursion = HIVE_MAX_SIG_CHECK_DEPTH,
      uint32_t max_membership = HIVE_MAX_AUTHORITY_MEMBERSHIP,
      uint32_t max_account_auths = HIVE_MAX_SIG_CHECK_ACCOUNTS,
      canonical_signature_type canon_type = fc::ecc::fc_canonical
      )const;

    void verify_authority(
      const chain_id_type& chain_id,
      const authority_getter& get_active,
      const authority_getter& get_owner,
      const authority_getter& get_posting,
      const witness_public_key_getter& get_witness_key,
      hive::protocol::pack_type pack,
      uint32_t max_recursion/* = HIVE_MAX_SIG_CHECK_DEPTH*/,
      uint32_t max_membership = HIVE_MAX_AUTHORITY_MEMBERSHIP,
      uint32_t max_account_auths = HIVE_MAX_SIG_CHECK_ACCOUNTS,
      canonical_signature_type canon_type = fc::ecc::fc_canonical
      )const;

    set<public_key_type> minimize_required_signatures(
      const chain_id_type& chain_id,
      const flat_set<public_key_type>& available_keys,
      const authority_getter& get_active,
      const authority_getter& get_owner,
      const authority_getter& get_posting,
      const witness_public_key_getter& get_witness_key,
      uint32_t max_recursion = HIVE_MAX_SIG_CHECK_DEPTH,
      uint32_t max_membership = HIVE_MAX_AUTHORITY_MEMBERSHIP,
      uint32_t max_account_auths = HIVE_MAX_SIG_CHECK_ACCOUNTS,
      canonical_signature_type canon_type = fc::ecc::fc_canonical
      ) const;

    flat_set<public_key_type> get_signature_keys( const chain_id_type& chain_id, canonical_signature_type/* = fc::ecc::fc_canonical*/, hive::protocol::pack_type pack )const;

    vector<signature_type> signatures;
    digest_type merkle_digest()const;

    void clear() { operations.clear(); signatures.clear(); }
  };

  /// @} transactions group

} } // hive::protocol

FC_REFLECT( hive::protocol::transaction, (ref_block_num)(ref_block_prefix)(expiration)(operations)(extensions) )
FC_REFLECT_DERIVED( hive::protocol::signed_transaction, (hive::protocol::transaction), (signatures) )
