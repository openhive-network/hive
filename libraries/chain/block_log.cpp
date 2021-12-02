#include <hive/chain/block_log.hpp>
#include <hive/chain/file_operation.hpp>

#include <hive/chain/index_file.hpp>
#include <hive/chain/block_log_file.hpp>

#include <fc/io/raw.hpp>

#include <unistd.h>
#include <boost/make_shared.hpp>

#include <fstream>

#ifndef MMAP_BLOCK_IO
  #define MMAP_BLOCK_IO
#endif

#ifdef MMAP_BLOCK_IO
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#endif

namespace hive { namespace chain {

  namespace detail {

    class block_log_impl
    {
      public:
        block_log_file  block_log_data;
        block_log_index block_log_idx;
        
    };

  } // end namespace detail

  block_log::block_log() : my( new detail::block_log_impl() )
  {
    my->block_log_data.storage.file_descriptor = -1;
  }

  block_log::~block_log()
  {
    if (my->block_log_data.storage.file_descriptor != -1)
      ::close( my->block_log_data.storage.file_descriptor );
  }

  void block_log::open( const fc::path& file )
  {
    close();

    my->block_log_data.storage.file = file;

    my->block_log_data.storage.file_descriptor = ::open(my->block_log_data.storage.file.generic_string().c_str(), O_RDWR | O_APPEND | O_CREAT | O_CLOEXEC, 0644);
    if( my->block_log_data.storage.file_descriptor == -1 )
      FC_THROW("Error opening block log file ${filename}: ${error}", ("filename", my->block_log_data.storage.file)("error", strerror(errno)));
    my->block_log_data.storage.block_log_size = file_operation::get_file_size( my->block_log_data.storage.file_descriptor );

    const ssize_t index_size = my->block_log_idx.open( file );

    /* On startup of the block log, there are several states the log file and the index file can be
      * in relation to eachother.
      *
      *                          Block Log
      *                     Exists       Is New
      *                 +------------+------------+
      *          Exists |    Check   |   Delete   |
      *   Index         |    Head    |    Index   |
      *    File         +------------+------------+
      *          Is New |   Replay   |     Do     |
      *                 |    Log     |   Nothing  |
      *                 +------------+------------+
      *
      * Checking the heads of the files has several conditions as well.
      *  - If they are the same, do nothing.
      *  - If the index file head is not in the log file, delete the index and replay.
      *  - If the index file head is in the log, but not up to date, replay from index head.
      */
    if( my->block_log_data.storage.block_log_size )
    {
      ilog( "Log is nonempty" );
      my->block_log_data.head.exchange(boost::make_shared<signed_block>(read_head()));

      boost::shared_ptr<signed_block> head_block = my->block_log_data.head.load();
      my->block_log_idx.prepare( index_size, head_block, my->block_log_data.storage );
    }
    else if( index_size )
    {
      my->block_log_idx.non_empty_idx_info();
    }
  }

  void block_log::rewrite(const fc::path& input_file, const fc::path& output_file, uint32_t max_block_num)
  {
    std::ifstream intput_block_stream(input_file.generic_string().c_str(), std::ios::in | std::ios::binary);
    std::ofstream output_block_stream(output_file.generic_string().c_str(), std::ios::out | std::ios::binary | std::ios::app);

    uint64_t pos = 0;
    uint64_t end_pos = 0;

    intput_block_stream.seekg(-sizeof(uint64_t), std::ios::end);
    intput_block_stream.read((char*)&end_pos, sizeof(end_pos));

    intput_block_stream.seekg(pos);

    uint32_t block_num = 0;

    while(pos < end_pos)
    {
      signed_block tmp;
      fc::raw::unpack(intput_block_stream, tmp);
      intput_block_stream.read((char*)&pos, sizeof(pos));

      uint64_t out_pos = output_block_stream.tellp();

      if(out_pos != pos)
        ilog("Block position mismatch");

      auto data = fc::raw::pack_to_vector(tmp);
      output_block_stream.write(data.data(), data.size());
      output_block_stream.write((char*)&out_pos, sizeof(out_pos));

      if(++block_num >= max_block_num)
        break;

      if(block_num % 1000 == 0)
        printf("Rewritten block: %u\r", block_num);
    }
  }

  void block_log::close()
  {
    if ( my->block_log_data.storage.file_descriptor != -1) {
      ::close(my->block_log_data.storage.file_descriptor);
      my->block_log_data.storage.file_descriptor = -1;
    }

    if( my->block_log_idx.close() ) {
      my->block_log_data.storage.file_descriptor = -1;
    }
    my->block_log_data.head.store(boost::shared_ptr<signed_block>());
  }

  bool block_log::is_open()const
  {
    return my->block_log_data.storage.file_descriptor != -1;
  }

  // threading guarantees:
  // - this function may only be called by one thread at a time
  // - It is safe to call `append` while any number of other threads 
  //   are reading the block log.
  // There is no real use-case for multiple writers so it's not worth
  // adding a lock to allow it.
  uint64_t block_log::append( const signed_block& b )
  {
    try
    {
      uint64_t block_start_pos = my->block_log_data.storage.block_log_size;
      std::vector<char> serialized_block = fc::raw::pack_to_vector(b);

      // what we write to the file is the serialized data, followed by the index of the start of the
      // serialized data.  Append that index so we can do it in a single write.
      unsigned serialized_byte_count = serialized_block.size();
      serialized_block.resize(serialized_byte_count + sizeof(uint64_t));
      *(uint64_t*)(serialized_block.data() + serialized_byte_count) = block_start_pos;

      file_operation::write_with_retry( my->block_log_data.storage.file_descriptor, serialized_block.data(), serialized_block.size() );
      my->block_log_data.storage.block_log_size += serialized_block.size();

      // add it to the index
      my->block_log_idx.append(&block_start_pos, sizeof(block_start_pos));

      // and update our cached head block
      boost::shared_ptr<signed_block> new_head = boost::make_shared<signed_block>(b);
      my->block_log_data.head.exchange(new_head);

      return block_start_pos;
    }
    FC_LOG_AND_RETHROW()
  }

  // threading guarantees:
  // no restrictions
  void block_log::flush()
  {
    // writes to file descriptors are flushed automatically
  }

  optional< signed_block > block_log::read_block_by_num( uint32_t block_num )const
  {
    try
    {
      // first, check if it's the current head block; if so, we can just return it.  If the
      // block number is less than than the current head, it's guaranteed to have been fully
      // written to the log+index
      boost::shared_ptr<signed_block> head_block = my->block_log_data.head.load();
      /// \warning ignore block 0 which is invalid, but old API also returned empty result for it (instead of assert).
      if (block_num == 0 || !head_block || block_num > head_block->block_num())
        return optional<signed_block>();
      if (block_num == head_block->block_num())
        return *head_block;
      // if we're still here, we know that it's in the block log, and the block after it is also
      // in the block log (which means we can determine its size)

      uint64_t _offset  = 0;
      uint64_t _size    = 0;
      my->block_log_idx.read( block_num, _offset, _size );

      return file_operation::read_block_from_offset_and_size( my->block_log_data.storage.file_descriptor, _offset, _size );
    }
    FC_CAPTURE_LOG_AND_RETHROW((block_num))
  }

  vector<signed_block> block_log::read_block_range_by_num( uint32_t first_block_num, uint32_t count )const
  {
    try
    {
      // first, check if the last block we want is the current head block; if so, we can 
      // will use it and then load the previous blocks from the block log
      boost::shared_ptr<signed_block> head_block = my->block_log_data.head.load();

      return my->block_log_idx.read_block_range( first_block_num, count, my->block_log_data.storage.file_descriptor, head_block );
    }
    FC_CAPTURE_LOG_AND_RETHROW((first_block_num)(count))
  }

  // not thread safe, but it's only called when opening the block log, we can assume we're the only thread accessing it
  signed_block block_log::read_head()const
  {
    try
    {
      ssize_t _actual_size = file_operation::get_file_size( my->block_log_data.storage.file_descriptor );

      // read the last int64 of the block log into `head_block_offset`, 
      // that's the index of the start of the head block
      FC_ASSERT(_actual_size >= (ssize_t)sizeof(uint64_t));
      uint64_t head_block_offset;
      file_operation::pread_with_retry(my->block_log_data.storage.file_descriptor, &head_block_offset, sizeof(head_block_offset), 
                                               _actual_size - sizeof(head_block_offset));

      return file_operation::read_block_from_offset_and_size( my->block_log_data.storage.file_descriptor, head_block_offset, _actual_size - head_block_offset - sizeof(head_block_offset) );
    }
    FC_LOG_AND_RETHROW()
  }

  const boost::shared_ptr<signed_block> block_log::head()const
  {
    return my->block_log_data.head.load();
  }

} } // hive::chain
