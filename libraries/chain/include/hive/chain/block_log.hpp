#pragma once
#include <fc/filesystem.hpp>
#include <hive/protocol/block.hpp>

#include <hive/chain/detail/block_attributes.hpp>
#include <hive/chain/block_log_artifacts.hpp>

#define BLOCKS_IN_SPLIT_BLOCK_LOG_FILE 1000000

extern "C"
{
  struct ZSTD_CCtx_s;
  typedef struct ZSTD_CCtx_s ZSTD_CCtx;
  struct ZSTD_DCtx_s;
  typedef struct ZSTD_DCtx_s ZSTD_DCtx;
}

namespace hive { namespace chain {

  /**
   * @brief Utility class facilitating block log (artifacts) configuration based on file paths.
   * 
   */
  class block_log_file_name_info
  {
  public:
    static uint32_t get_first_block_num_for_file_name(const fc::path& block_log_path );

    /**
     * @brief Get the block log file name of nth part file.
     *        Useful for multi-part split log only.     * 
     * @return string combining filename and its extension 
     */
    static std::string get_nth_part_file_name(uint32_t part_number);

    /**
     * @brief Match path to split block log name pattern to deduct part number.
     * @return part number or 0 (when not part file).
     */
    static uint32_t is_part_file( const fc::path& file );

    static bool is_block_log_file_name( const fc::path& file )
    {
      return file.filename().string() == _legacy_file_name ||
             is_part_file( file );
    }

  public:
    static const std::string _legacy_file_name;

  private:
    static const std::string _split_file_name_core;
    static const size_t _split_file_name_extension_length = 4;
  }; // block_log_file_name_info

  // struct serialized_block_data
  // {
  //   std::unique_ptr<char[]> data;
  //   size_t size;
  //   block_log_artifacts::artifacts_t>
  // };

  using namespace hive::protocol;

  class full_block_type;

  namespace detail { class block_log_impl; }

  /* WARNING - use directly only when you know what you're doing (see e.g. block_log_util.cpp),
    *          otherwise use reader/writer wrappers that encapsulate all problems related to 
    *          multi-file block logs etc. See block_log_manager_t.
    *
    * The block log is an external append only log of the blocks. Blocks should only be written
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

      block_log( appbase::application& app );
      ~block_log();

      void open( const fc::path& file, hive::chain::blockchain_worker_thread_pool& thread_pool, bool read_only = false, bool auto_open_artifacts = true );
      void open_and_init( const fc::path& file,
                          bool read_only,
                          bool enable_compression,
                          int compression_level,
                          bool enable_block_log_auto_fixing, 
                          hive::chain::blockchain_worker_thread_pool& thread_pool );
      void close();
      bool is_open()const;

      fc::path get_log_file() const;
      fc::path get_artifacts_file() const;

      uint64_t append(const std::shared_ptr<full_block_type>& full_block, const bool is_at_live_sync);
      uint64_t append_raw(uint32_t block_num, const char* raw_block_data, size_t raw_block_size, const block_attributes_t& flags, const bool is_at_live_sync);
      uint64_t append_raw(uint32_t block_num, const char* raw_block_data, size_t raw_block_size, const block_attributes_t& flags, const block_id_type& block_id, const bool is_at_live_sync);

      void flush();
      std::tuple<std::unique_ptr<char[]>, size_t, block_log_artifacts::artifacts_t> read_raw_block_data_by_num(uint32_t block_num) const;
      static std::tuple<std::unique_ptr<char[]>, size_t> decompress_raw_block(std::tuple<std::unique_ptr<char[]>, size_t, block_attributes_t>&& raw_block_data_tuple);
      static std::tuple<std::unique_ptr<char[]>, size_t> decompress_raw_block(const char* raw_block_data, size_t raw_block_size, block_attributes_t attributes);

      /** Allows to read just block_id for block identified by given block number. 
      *   \warning Can return empty `block_id_type` if block_num is out of valid range.
      */
      hive::protocol::block_id_type    read_block_id_by_num(uint32_t block_num) const;
      std::shared_ptr<full_block_type> read_block_by_num(uint32_t block_num) const;
      std::shared_ptr<full_block_type> read_block_by_offset(uint64_t offset, size_t size, block_attributes_t attributes) const;
      std::vector<std::shared_ptr<full_block_type>> read_block_range_by_num(uint32_t first_block_num, uint32_t count) const;

      std::tuple<std::unique_ptr<char[]>, size_t, block_log::block_attributes_t> read_raw_head_block() const;
      std::shared_ptr<full_block_type> read_head() const;
      std::shared_ptr<full_block_type> head() const;

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

      /// return true to continue processing, false to stop iteration.
      typedef std::function<bool(const std::shared_ptr<full_block_type>&, const uint64_t, const uint32_t, const block_attributes_t)> artifacts_generation_processor;
      /// processes blocks in REVERSE order.  This only reads the block_log file, and can be used for rebuilding the artifacts/index file
      void read_blocks_data_for_artifacts_generation(artifacts_generation_processor processor, const uint32_t target_block_number, const uint32_t starting_block_number,
                                                     const fc::optional<uint64_t> starting_block_position = fc::optional<uint64_t>()) const;

      /// return true to continue processing, false to stop iteration.
      typedef std::function<bool(const std::shared_ptr<full_block_type>&)> block_processor_t;
      // determines what processing for_each_block() asks the blockchain worker threads to perform
      enum class for_each_purpose { replay, decompressing };
      // process blocks in forward order, [starting_block_number, ending_block_number]
      void for_each_block(uint32_t starting_block_number, uint32_t ending_block_number,
                          block_processor_t processor,
                          for_each_purpose purpose,
                          hive::chain::blockchain_worker_thread_pool& thread_pool) const;

      // shorten the block log & artifacts file
      void truncate(uint32_t new_head_block_num);
    private:
      void sanity_check(const bool read_only);
      std::unique_ptr<detail::block_log_impl> my;

      appbase::application& theApp;
  };

} }

