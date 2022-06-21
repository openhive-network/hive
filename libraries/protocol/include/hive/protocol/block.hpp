#pragma once
#include <hive/protocol/block_header.hpp>
#include <hive/protocol/transaction.hpp>

namespace hive { namespace protocol {

  struct signed_block : public signed_block_header
  {
    checksum_type legacy_calculate_merkle_root()const;
    vector<signed_transaction> transactions;
  };

} } // hive::protocol

FC_REFLECT_DERIVED( hive::protocol::signed_block, (hive::protocol::signed_block_header), (transactions) )
