#include <hive/protocol/block.hpp>
#include <hive/protocol/merkle.hpp>
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
    vector<digest_type> ids;
    ids.resize( transactions.size() );
    for( uint32_t i = 0; i < transactions.size(); ++i )
      ids[i] = transactions[i].merkle_digest(hive::protocol::pack_type::legacy);

    return calculate_merkle_root( std::move( ids ) );
  }

} } // hive::protocol
