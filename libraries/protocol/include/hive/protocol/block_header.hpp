#pragma once
#include <hive/protocol/base.hpp>
#include <hive/protocol/operation_util.hpp>

namespace hive { namespace protocol {

  typedef static_variant<
    void_t,
    version,                // Normal witness version reporting, for diagnostics and voting
    hardfork_version_vote   // Voting for the next hardfork to trigger
  >                                block_header_extensions;

  typedef flat_set<block_header_extensions > block_header_extensions_type;

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
