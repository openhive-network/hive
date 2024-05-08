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
