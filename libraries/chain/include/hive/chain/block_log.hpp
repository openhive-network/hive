#pragma once
#include <fc/filesystem.hpp>
#include <hive/protocol/block.hpp>

#include <hive/chain/detail/block_attributes.hpp>
#include <hive/chain/block_log_artifacts.hpp>

extern "C"
{
  struct ZSTD_CCtx_s;
  typedef struct ZSTD_CCtx_s ZSTD_CCtx;
  struct ZSTD_DCtx_s;
  typedef struct ZSTD_DCtx_s ZSTD_DCtx;
}

namespace hive { namespace chain {

  // struct serialized_block_data
  // {
  //   std::unique_ptr<char[]> data;
  //   size_t size;
  //   block_log_artifacts::artifacts_t>
  // };

  using namespace hive::protocol;

  class full_block_type;

  namespace detail { class block_log_impl; }

  /* The block log is an external append only log of the blocks. Blocks should only be written
    * to the log after they irreverisble as the log is append only. The log is a doubly linked
    * list of blocks. There is a secondary index file of only block positions that enables O(1)
    * random access lookup by block number.
    *
    * +---------+----------------+---------+----------------+-----+------------+-------------------+
    * | Block 1 | Pos of Block 1 | Block 2 | Pos of Block 2 | ... | Head Block | Pos of Head Block |
    * +---------+----------------+---------+----------------+-----+------------+-------------------+
    *
    * +----------------+----------------+-----+-------------------+
    * | Pos of Block 1 | Pos of Block 2 | ... | Pos of Head Block |
    * +----------------+----------------+-----+-------------------+
    *
    * The block log can be walked in order by deserializing a block, skipping 8 bytes, deserializing a
    * block, repeat... The head block of the file can be found by seeking to the position contained
    * in the last 8 bytes the file. The block log can be read backwards by jumping back 8 bytes, following
    * the position, reading the block, jumping back 8 bytes, etc.
    *
    * Blocks can be accessed at random via block number through the index file. Seek to 8 * (block_num - 1)
    * to find the position of the block in the main file.
    *
    * The main file is the only file that needs to persist. The index file can be reconstructed during a
    * linear scan of the main file.
    */

  class block_log {
    public:
      using block_flags=detail::block_flags;
      using block_attributes_t=detail::block_attributes_t;

      using block_id_type=hive::protocol::block_id_type;

      block_log();
      ~block_log();

      void open( const fc::path& file, bool read_only = false, bool auto_open_artifacts = true );

      void close();
      bool is_open()const;

      uint64_t append(const std::shared_ptr<full_block_type>& full_block);
      uint64_t append_raw(uint32_t block_num, const char* raw_block_data, size_t raw_block_size, const block_attributes_t& flags);
      uint64_t append_raw(uint32_t block_num, const char* raw_block_data, size_t raw_block_size, const block_attributes_t& flags, const block_id_type& block_id);

      void flush();
      std::tuple<std::unique_ptr<char[]>, size_t, block_log_artifacts::artifacts_t> read_raw_block_data_by_num(uint32_t block_num) const;
      static std::tuple<std::unique_ptr<char[]>, size_t> decompress_raw_block(std::tuple<std::unique_ptr<char[]>, size_t, block_attributes_t>&& raw_block_data_tuple);
      static std::tuple<std::unique_ptr<char[]>, size_t> decompress_raw_block(const char* raw_block_data, size_t raw_block_size, block_attributes_t attributes);

      std::shared_ptr<full_block_type> read_block_by_num(uint32_t block_num) const;
      std::shared_ptr<full_block_type> read_block_by_offset(uint64_t offset, size_t size, block_attributes_t attributes) const;
      std::vector<std::shared_ptr<full_block_type>> read_block_range_by_num(uint32_t first_block_num, uint32_t count) const;

      std::tuple<std::unique_ptr<char[]>, size_t, block_log::block_attributes_t> read_raw_head_block() const;
      std::shared_ptr<full_block_type> read_head() const;
      std::shared_ptr<full_block_type> head() const;
      void set_compression(bool enabled);
      void set_compression_level(int level);

      static std::tuple<std::unique_ptr<char[]>, size_t> compress_block_zstd(const char* uncompressed_block_data, size_t uncompressed_block_size, std::optional<uint8_t> dictionary_number, 
                                                                             fc::optional<int> compression_level = fc::optional<int>(), 
                                                                             fc::optional<ZSTD_CCtx*> compression_context = fc::optional<ZSTD_CCtx*>());
      static std::tuple<std::unique_ptr<char[]>, size_t> decompress_block_zstd(const char* compressed_block_data, size_t compressed_block_size, 
                                                                               std::optional<uint8_t> dictionary_number = std::optional<int>(), 
                                                                               fc::optional<ZSTD_DCtx*> decompression_context_for_reuse = fc::optional<ZSTD_DCtx*>());

      /// Functor takes: block_num, serialized_block_data_size, block_log_file_offset, block_attributes. 
      /// It should return true to continue processing, false to stop iteration.
      typedef std::function<bool(uint32_t, uint32_t, uint64_t, const block_attributes_t&)> block_info_processor_t;
      /// Allows to process blocks in REVERSE order.
      void for_each_block_position(block_info_processor_t processor) const;


      // for testing, does disk reads in a similar manner to above, but in the FORWARDS order
      void for_each_block_position_forwards(block_info_processor_t processor) const;

      /// return true to continue processing, false to stop iteration.
      typedef std::function<bool(uint32_t, const std::shared_ptr<full_block_type>&, uint64_t, block_attributes_t)> block_processor_t;
      /// processes blocks in REVERSE order
      void for_each_block(block_processor_t processor) const;

    private:
      std::unique_ptr<detail::block_log_impl> my;
  };

} }

