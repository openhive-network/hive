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

      virtual uint64_t get_last_element_num( uint32_t block_num, uint64_t block_pos ) = 0;
      virtual void read_last_element_num( uint64_t last_element_num ) = 0;

    public:

      storage_description storage;

      base_index( const storage_description::storage_type val, const std::string& file_name_ext_val );
      virtual ~base_index();

      void open( const fc::path& file );
      void open();
      void prepare( const boost::shared_ptr<signed_block>& head_block, const storage_description& desc );
      void close();

      void non_empty_idx_info();
      void read( uint32_t block_num, uint64_t& offset, uint64_t& size );
      vector<signed_block> read_block_range( uint32_t first_block_num, uint32_t count, const storage_description& block_log_storage, const boost::shared_ptr<signed_block>& head_block );

      virtual void check_consistency( uint32_t total_size ) = 0;
      virtual void write( std::fstream& stream, const signed_block& block, uint64_t position ) = 0;
      virtual void append( const signed_block& block, uint64_t position ) = 0;
      virtual std::tuple< optional<block_id_type>, optional<public_key_type> > read_data_by_num( uint32_t block_num );
      virtual std::map< uint32_t, std::tuple< optional<block_id_type>, optional<public_key_type> > > read_data_range_by_num( uint32_t first_block_num, uint32_t count );
  };

  class block_log_index: public base_index
  {
    protected:

      uint64_t get_last_element_num( uint32_t block_num, uint64_t block_pos ) override;
      void read_last_element_num( uint64_t last_element_num ) override;

    public:

      block_log_index( const storage_description::storage_type val, const std::string& file_name_ext_val );
      ~block_log_index();

      void check_consistency( uint32_t total_size ) override;
      void write( std::fstream& stream, const signed_block& block, uint64_t position ) override;
      void append( const signed_block& block, uint64_t position ) override;
  };

  template<uint32_t ELEMENT_SIZE>
  class custom_index: public base_index
  {
    protected:

      const uint32_t element_size = ELEMENT_SIZE;

      using buffer_type = std::array<char, ELEMENT_SIZE>;
      buffer_type buffer;

    public:

      custom_index( const storage_description::storage_type val, const std::string& file_name_ext_val );
      ~custom_index();

      void check_consistency( uint32_t total_size ) override;
  };

  struct sizes_type
  {
    const uint32_t BLOCK_NUMBER_SIZE  = sizeof(uint32_t);
    const uint32_t BLOCK_ID_SIZE      = sizeof(block_id_type);
    const uint32_t PUBLIC_KEY_SIZE    = sizeof(fc::ecc::public_key_data);
  };

  constexpr sizes_type sizes = sizes_type();
  
  class block_id_witness_public_key: public custom_index<sizes.BLOCK_NUMBER_SIZE + sizes.BLOCK_ID_SIZE + sizes.PUBLIC_KEY_SIZE>
  {
    private:

      uint64_t get_last_element_num( uint32_t block_num, uint64_t block_pos ) override;
      void read_last_element_num( uint64_t last_element_num ) override;
      std::tuple< optional<block_id_type>, optional<public_key_type> > read_data_from_buffer( uint32_t block_num, char* buffer );

      void write_impl( const signed_block& block, std::function<void(char*, uint32_t)> write_func );

    public:

      block_id_witness_public_key( const storage_description::storage_type val, const std::string& file_name_ext_val );
      ~block_id_witness_public_key();

      void write( std::fstream& stream, const signed_block& block, uint64_t position ) override;
      void append( const signed_block& block, uint64_t position ) override;
      std::tuple< optional<block_id_type>, optional<public_key_type> > read_data_by_num( uint32_t block_num ) override;
      std::map< uint32_t, std::tuple< optional<block_id_type>, optional<public_key_type> > > read_data_range_by_num( uint32_t first_block_num, uint32_t count ) override;
  };

} } // hive::chain
