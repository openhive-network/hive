#pragma once
#include <hive/protocol/block_header.hpp>
#include <hive/protocol/transaction.hpp>

namespace hive { namespace protocol {

  struct merkle_root_calculator
  {
    template<typename transaction_collection>
    static checksum_type calculate_merkle_root( const transaction_collection& transactions )
    {
      if( transactions.size() == 0 )
        return checksum_type();

      vector<digest_type> ids;
      ids.resize( transactions.size() );
      for( uint32_t i = 0; i < transactions.size(); ++i )
        ids[i] = transactions[i].merkle_digest();

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
  };

  struct signed_block : public signed_block_header
  {
    checksum_type calculate_merkle_root()const;
    vector<signed_transaction> transactions;
  };

} } // hive::protocol

FC_REFLECT_DERIVED( hive::protocol::signed_block, (hive::protocol::signed_block_header), (transactions) )
