#pragma once

#include <hive/chain/block_log.hpp>
#include <hive/chain/block_log_writer_common.hpp>

namespace appbase {
  class application;
}

namespace hive { namespace chain {

  /// Multiple file implementation of block_log_writer_common.
  class split_file_block_log_writer : public block_log_writer_common
  {
  protected:
    /// Required by block_log_reader_common.
    virtual const std::shared_ptr<full_block_type> get_head_block() const;
    /// Required by block_log_reader_common.
    virtual block_id_type read_block_id_by_num( uint32_t block_num ) const override;
    /// Required by block_log_reader_common.
    virtual block_range_t read_block_range_by_num( uint32_t starting_block_num, uint32_t count ) const;

  public:
    split_file_block_log_writer( int block_log_split, appbase::application& app,
                                 blockchain_worker_thread_pool& thread_pool );
    virtual ~split_file_block_log_writer() = default;

    /// Required by block_log_reader_common.
    virtual void close_log() override;
    /// Required by block_log_reader_common.
    virtual std::tuple<std::unique_ptr<char[]>, size_t, block_attributes_t> read_raw_head_block() const override;
    /// Required by block_log_reader_common.
    virtual std::tuple<std::unique_ptr<char[]>, size_t, block_log_artifacts::artifacts_t> read_raw_block_data_by_num(uint32_t block_num) const override;

    /// Required by block_log_writer_common.
    virtual void open_and_init( const block_log_open_args& bl_open_args ) override;
    /// Required by block_log_writer_common.
    virtual void open_and_init( const fc::path& path, bool read_only ) override;
    /// Required by block_log_writer_common.
    virtual void append( const std::shared_ptr<full_block_type>& full_block, const bool is_at_live_sync ) override;
    /// Required by block_log_writer_common.
    virtual void flush_head_log() override;
    /// Required by block_log_writer_common.
    virtual uint64_t append_raw( uint32_t block_num, const char* raw_block_data, size_t raw_block_size,
                                 const block_attributes_t& flags, const bool is_at_live_sync ) override;

    /// block_read_i method not implemented by block_log_reader_common.
    virtual std::shared_ptr<full_block_type> read_block_by_num( uint32_t block_num ) const override;

    /// block_read_i method not implemented by block_log_reader_common.
    virtual void process_blocks( uint32_t starting_block_number, uint32_t ending_block_number,
                                 block_processor_t processor, hive::chain::blockchain_worker_thread_pool& thread_pool ) const override;

  private:
    void common_open_and_init( std::optional< bool > read_only );
    void internal_open_and_init( block_log* the_log, const fc::path& path, bool read_only );

    bool is_last_number_of_the_file( uint32_t block_num ) const
    {
      return block_num > 0 && 
            ( block_num % _max_blocks_in_log_file == 0 );
    }

    /** Returns 1 for 0,
     *  1 for [1 .. _max_blocks_in_log_file] &
     *  N for [1+(N-1)*_max_blocks_in_log_file .. N*_max_blocks_in_log_file]
     */
    uint32_t get_part_number_for_block( uint32_t block_num ) const
    {
      return block_num == 0 ? 1 :
        ( (uint64_t)block_num + (uint64_t)_max_blocks_in_log_file -1 ) / _max_blocks_in_log_file;
    }

    const block_log* get_block_log_corresponding_to( uint32_t block_num ) const;

    using append_t = std::function< void( block_log* log ) >;
    void internal_append( uint32_t block_num, append_t do_appending);

    /**
     * Throw if the value of lowest part file number is wrong in given implementation.
     * @return actual needed tail part number.
     */
    uint32_t validate_tail_part_number( uint32_t tail_part_number, uint32_t head_part_number ) const;
    /// Called when a new log object (corresponding to a new part file) was pushed to their container.
    void rotate_part_files( uint32_t new_part_number );

  protected:
    appbase::application&           _app;
    blockchain_worker_thread_pool&  _thread_pool;
    const uint32_t                  _max_blocks_in_log_file = 0;
    const int                       _block_log_split = 0;
    block_log_open_args             _open_args;
    std::deque< block_log* >        _logs;
  };

} }
