#include <hive/protocol/merkle.hpp>

#include <fc/io/raw.hpp>

#include <utility>

namespace hive { namespace protocol {

checksum_type calculate_merkle_root( std::vector<digest_type> ids )
{
  if( ids.empty() )
    return checksum_type();

  std::vector<digest_type>::size_type current_number_of_hashes = ids.size();
  while( current_number_of_hashes > 1 )
  {
    // hash ID's in pairs
    uint32_t i_max = current_number_of_hashes - ( current_number_of_hashes & 1 );
    uint32_t k = 0;

    for( uint32_t i = 0; i < i_max; i += 2 )
      ids[ k++ ] = digest_type::hash( std::make_pair( ids[ i ], ids[ i + 1 ] ) );

    if( current_number_of_hashes & 1 )
      ids[ k++ ] = ids[ i_max ];
    current_number_of_hashes = k;
  }

  return checksum_type::hash( ids[ 0 ] );
}

} } // hive::protocol
