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

  void signed_block_header::legacy_sign( const fc::ecc::private_key& signer, fc::ecc::canonical_signature_type canon_type )
  {
    witness_signature = signer.sign_compact( legacy_digest(), canon_type );
  }
  // just here for unit tests, actual function is in full_block
  checksum_type signed_block::legacy_calculate_merkle_root()const
  {
    if( transactions.size() == 0 )
      return checksum_type();

    vector<digest_type> ids;
    ids.resize( transactions.size() );
    for( uint32_t i = 0; i < transactions.size(); ++i )
      ids[i] = transactions[i].merkle_digest(hive::protocol::pack_type::legacy);

    hive::protocol::serialization_mode_controller::pack_guard guard( hive::protocol::pack_type::legacy );

    vector<digest_type>::size_type current_number_of_hashes = ids.size();
    while( current_number_of_hashes > 1 )
    {
      // hash ID's in pairs
      uint32_t i_max = current_number_of_hashes - (current_number_of_hashes&1);
      uint32_t k = 0;

      for( uint32_t i = 0; i < i_max; i += 2 )
        ids[k++] = digest_type::hash( std::make_pair( ids[i], ids[i+1] ) );

      if( current_number_of_hashes&1 )
        ids[k++] = ids[i_max];
      current_number_of_hashes = k;
    }
    return checksum_type::hash( ids[0] );
  }

} } // hive::protocol
