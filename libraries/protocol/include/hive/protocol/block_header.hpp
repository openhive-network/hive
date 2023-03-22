#pragma once
#include <hive/protocol/required_automated_actions.hpp>
#include <hive/protocol/optional_automated_actions.hpp>
#include <hive/protocol/base.hpp>

#pragma GCC push_options
#pragma GCC optimize("O0")

namespace hive { namespace protocol {

  typedef vector< required_automated_action > required_automated_actions;
  typedef vector< optional_automated_action > optional_automated_actions;

  FC_TODO( "Remove when automated actions are created" )
  typedef static_variant<
    void_t,
    version,                // Normal witness version reporting, for diagnostics and voting
    hardfork_version_vote   // Voting for the next hardfork to trigger
#ifdef IS_TEST_NET
,
    required_automated_actions,
    optional_automated_actions
#endif
    >                                block_header_extensions;

  typedef flat_set<block_header_extensions > block_header_extensions_type;

  // note: the functions that had to compute the block digest have been deprecated -- 
  // as of hf26, there can be multiple valid serializations for transactions, and
  // you can't tell which is correct just by looking the in-memory structures.
  // The legacy versions of digest(), id(), signee() have been preserved mostly to
  // support report_over_production_operation (no longer in use)

  struct block_header
  {
    digest_type                   legacy_digest()const; // was: digest, use instead full_block_type::get_digest()
    block_id_type                 previous;
    uint32_t                      block_num()const { return num_from_id(previous) + 1; }
    fc::time_point_sec            timestamp;
    string                        witness;
    checksum_type                 transaction_merkle_root;
    block_header_extensions_type  extensions;

    static uint32_t num_from_id(const block_id_type& id);
  };

  struct signed_block_header : public block_header
  {
    block_id_type              legacy_id()const; // was: id(), use instead full_block_type::get_block_id()
    static fc::ecc::public_key signee(const signature_type& witness_signature, const digest_type& digest, fc::ecc::canonical_signature_type canon_type);
    fc::ecc::public_key        legacy_signee( fc::ecc::canonical_signature_type canon_type = fc::ecc::bip_0062 )const;// was: signee(), use instead full_block_type::get_signing_key()
    void                       legacy_sign( const fc::ecc::private_key& signer, fc::ecc::canonical_signature_type canon_type = fc::ecc::bip_0062 );

    bool                       validate_signee( const fc::ecc::public_key& expected_signee, const digest_type& digest, fc::ecc::canonical_signature_type canon_type )const;

    signature_type             witness_signature;
  };


} } // hive::protocol

namespace fc {

using hive::protocol::block_header_extensions;
template<>
struct serialization_functor< block_header_extensions >
{
  bool operator()( const fc::variant& v, block_header_extensions& s ) const
  {
    return extended_serialization_functor< block_header_extensions >().serialize( v, s );
  }
};

template<>
struct variant_creator_functor< block_header_extensions >
{
  template<typename T>
  fc::variant operator()( const T& v ) const
  {
    return extended_variant_creator_functor< block_header_extensions >().create( v );
  }
};

}

FC_REFLECT_TYPENAME( hive::protocol::block_header_extensions )

FC_REFLECT( hive::protocol::block_header, (previous)(timestamp)(witness)(transaction_merkle_root)(extensions) )
FC_REFLECT_DERIVED( hive::protocol::signed_block_header, (hive::protocol::block_header), (witness_signature) )

#pragma GCC pop_options
