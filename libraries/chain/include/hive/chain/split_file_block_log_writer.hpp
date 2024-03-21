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
    split_file_block_log_writer( appbase::application& app, blockchain_worker_thread_pool& thread_pool );
    virtual ~split_file_block_log_writer() = default;

    /// Required by block_log_writer_common.
    virtual void open_and_init( const block_log_open_args& bl_open_args ) override;
    /// Required by block_log_writer_common.
    virtual void close_log() override;
    /// Required by block_log_writer_common.
    virtual void append( const std::shared_ptr<full_block_type>& full_block, const bool is_at_live_sync ) override;
    /// Required by block_log_writer_common.
    virtual void flush_head_log() override;

    /// block_read_i method not implemented by block_log_reader_common.
    virtual std::shared_ptr<full_block_type> read_block_by_num( uint32_t block_num ) const override;

    /// block_read_i method not implemented by block_log_reader_common.
    virtual void process_blocks( uint32_t starting_block_number, uint32_t ending_block_number,
                                 block_processor_t processor, hive::chain::blockchain_worker_thread_pool& thread_pool ) const override;

  private:
    void open_and_init( block_log* the_log, const fc::path& path, bool read_only );

    bool is_last_number_of_the_file( uint32_t block_num ) const
    {
      return block_num > 0 && 
            ( block_num % BLOCKS_IN_SPLIT_BLOCK_LOG_FILE == 0 );
    }

    /** Returns 1 for 0,
     *  1 for [1 .. BLOCKS_IN_SPLIT_BLOCK_LOG_FILE] &
     *  N for [1+(N-1)*BLOCKS_IN_SPLIT_BLOCK_LOG_FILE .. N*BLOCKS_IN_SPLIT_BLOCK_LOG_FILE]
     */
    uint32_t get_part_number_for_block( uint32_t block_num ) const
    {
      return block_num == 0 ? 1 :
        ( block_num + BLOCKS_IN_SPLIT_BLOCK_LOG_FILE -1 ) / BLOCKS_IN_SPLIT_BLOCK_LOG_FILE;
    }

    const block_log* get_block_log_corresponding_to( uint32_t block_num ) const;

  private:
    appbase::application&           _app;
    blockchain_worker_thread_pool&  _thread_pool;
    block_log_open_args             _open_args;
    std::deque< block_log* >        _logs;
  };

} }
