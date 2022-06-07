#pragma once

#include <hive/protocol/block.hpp>

#include <hive/chain/signed_transaction_transporter.hpp>

namespace hive { namespace chain {

using hive::protocol::signed_block_header;
using hive::protocol::signed_block;

class signed_block_transporter
{
  public:

    signed_block_header block_header;

    vector<signed_transaction_transporter> transactions;

    signed_block_transporter(){}
    explicit signed_block_transporter( const signed_block& block );

    protocol::checksum_type calculate_merkle_root()const;
};

} } // hive::chain

FC_REFLECT( hive::chain::signed_block_transporter, (block_header)(transactions) )

namespace fc { namespace raw {
  
template< typename Stream >
inline void pack( Stream& s, const hive::chain::signed_block_transporter& value )
{
  pack( s, value.block_header );
  pack( s, value.transactions );
}

template< typename Stream >
inline void unpack( Stream& s, hive::chain::signed_block_transporter& value, uint32_t )
{
  unpack( s, value.block_header );
  unpack( s, value.transactions );
}

} }// fc::raw
