#include <hive/chain/signed_block_transporter.hpp>

#include <hive/protocol/misc_utilities.hpp>

using hive::protocol::pack_type;

namespace hive { namespace chain {

  signed_block_transporter::signed_block_transporter( const signed_block& block )
                            : block_header( block )
  {
    for( const auto& trx : block.transactions )
    {
      transactions.emplace_back( signed_transaction_transporter( trx, pack_type::legacy ) );
    }
  }

  protocol::checksum_type signed_block_transporter::calculate_merkle_root() const
  {
    return protocol::merkle_root_calculator::calculate_merkle_root( transactions );
  }

} } // hive::chain
