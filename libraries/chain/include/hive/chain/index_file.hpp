#pragma once

#include <hive/protocol/block.hpp>

#include <hive/chain/storage_description.hpp>

#include <fc/filesystem.hpp>

#include <boost/make_shared.hpp>

namespace hive { namespace chain {

  using namespace hive::protocol;

  class base_index
  {
    protected:

      virtual void read_blocks_number( uint64_t block_pos ) = 0;

    public:

      storage_description storage;

      base_index( const storage_description::storage_type val, const std::string& file_name_ext_val );
      virtual ~base_index();

      virtual void check_consistency( uint32_t total_size ) = 0;
      virtual void write( std::fstream& stream, const signed_block& block, uint64_t position ) = 0;

      void open( const fc::path& file );
      void open();
      void prepare( const boost::shared_ptr<signed_block>& head_block, const storage_description& desc );
      void close();

      void non_empty_idx_info();
      void append( const void* buf, size_t nbyte );
      void read( uint32_t block_num, uint64_t& offset, uint64_t& size );
      vector<signed_block> read_block_range( uint32_t first_block_num, uint32_t count, int block_log_fd, const boost::shared_ptr<signed_block>& head_block );
  };

  class block_log_index: public base_index
  {
    protected:

      void read_blocks_number( uint64_t block_pos ) override;

    public:

      block_log_index( const storage_description::storage_type val, const std::string& file_name_ext_val );
      ~block_log_index();

      void check_consistency( uint32_t total_size ) override;
      void write( std::fstream& stream, const signed_block& block, uint64_t position ) override;
  };

  template<uint32_t ELEMENT_SIZE_VALUE>
  class custom_index: public base_index
  {
    protected:

      const uint32_t ELEMENT_SIZE = sizeof(block_id_type) + sizeof(fc::ecc::public_key_data) + sizeof(uint64_t);
      void read_blocks_number( uint64_t block_pos ) override;

    public:

      custom_index( const storage_description::storage_type val, const std::string& file_name_ext_val );
      ~custom_index();

      void check_consistency( uint32_t total_size ) override;
  };
  
  constexpr uint32_t block_id_witness_public_key_size = 49;

  class block_id_witness_public_key: public custom_index<block_id_witness_public_key_size>
  {
    public:

      block_id_witness_public_key( const storage_description::storage_type val, const std::string& file_name_ext_val );
      ~block_id_witness_public_key();

      void write( std::fstream& stream, const signed_block& block, uint64_t position ) override;
  };

} } // hive::chain
