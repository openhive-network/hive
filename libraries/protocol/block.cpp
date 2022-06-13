#include <hive/protocol/block.hpp>
#include <fc/io/raw.hpp>
#include <fc/bitutil.hpp>
#include <algorithm>

namespace hive { namespace protocol {
  digest_type block_header::legacy_digest()const
  {
    hive::protocol::serialization_mode_controller::pack_guard guard( hive::protocol::pack_type::legacy );
    return digest_type::hash(*this);
  }

  uint32_t block_header::num_from_id(const block_id_type& id)
  {
    return fc::endian_reverse_u32(id._hash[0]);
  }

  block_id_type signed_block_header::legacy_id()const
  {
    hive::protocol::serialization_mode_controller::pack_guard guard( hive::protocol::pack_type::legacy );
    auto tmp = fc::sha224::hash( *this );
    tmp._hash[0] = fc::endian_reverse_u32(block_num()); // store the block num in the ID, 160 bits is plenty for the hash
    static_assert( sizeof(tmp._hash[0]) == 4, "should be 4 bytes" );
    block_id_type result;
    memcpy(result._hash, tmp._hash, std::min(sizeof(result), sizeof(tmp)));
    return result;
  }

  /* static */ fc::ecc::public_key signed_block_header::signee( const signature_type& witness_signature, const digest_type& digest, fc::ecc::canonical_signature_type canon_type )
  {
    return fc::ecc::public_key( witness_signature, digest, canon_type );
  }

  fc::ecc::public_key signed_block_header::legacy_signee( fc::ecc::canonical_signature_type canon_type )const
  {
    return signee(witness_signature, legacy_digest(), canon_type);
  }

  bool signed_block_header::validate_signee( const fc::ecc::public_key& expected_signee, const digest_type& digest, fc::ecc::canonical_signature_type canon_type )const
  {
    return signee(witness_signature, digest, canon_type) == expected_signee;
  }

} } // hive::protocol
