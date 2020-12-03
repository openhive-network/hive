#include <hive/chain/block_log.hpp>
#include <fstream>
#include <fc/io/raw.hpp>

#include <appbase/application.hpp>

#include <boost/thread/mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/interprocess/sync/lock_options.hpp>

#include <unistd.h>
#include <boost/smart_ptr/atomic_shared_ptr.hpp>
#include <boost/make_shared.hpp>

#define LOG_READ  (std::ios::in | std::ios::binary)
#define LOG_WRITE (std::ios::out | std::ios::binary | std::ios::app)

namespace hive { namespace chain {

  typedef boost::interprocess::scoped_lock< boost::mutex > scoped_lock;

  boost::interprocess::defer_lock_type defer_lock;

  namespace detail {
    class block_log_impl {
      public:
        boost::atomic_shared_ptr<signed_block> head;

        // these don't change after opening, don't need locking
        int block_log_fd;
        int block_index_fd;
        fc::path block_file;
        fc::path index_file;

        static void write_with_retry(int filedes, const void* buf, size_t nbyte);
        static void pwrite_with_retry(int filedes, const void* buf, size_t nbyte, off_t offset);
        static void pread_with_retry(int filedes, void* buf, size_t nbyte, off_t offset);

        // only accessed when appending a block, doesn't need locking
        ssize_t block_log_size;

        signed_block read_block_from_offset_and_size(uint64_t offset, uint64_t size);
    };

    void block_log_impl::write_with_retry(int fd, const void* buf, size_t nbyte)
    {
      for (;;)
      {
        ssize_t bytes_written = write(fd, buf, nbyte);
        if (bytes_written == -1)
          FC_THROW("Error writing ${nbytes} to file: ${error}", 
                   ("nbyte", nbyte)("error", strerror(errno)));
        if (bytes_written == (ssize_t)nbyte)
          return;
        buf = ((const char*)buf) + bytes_written;
        nbyte -= bytes_written;
      }
    }

    void block_log_impl::pwrite_with_retry(int fd, const void* buf, size_t nbyte, off_t offset)
    {
      for (;;)
      {
        ssize_t bytes_written = pwrite(fd, buf, nbyte, offset);
        if (bytes_written == -1)
          FC_THROW("Error writing ${nbytes} to file at offset ${offset}: ${error}", 
                   ("nbyte", nbyte)("offset", offset)("error", strerror(errno)));
        if (bytes_written == (ssize_t)nbyte)
          return;
        buf = ((const char*)buf) + bytes_written;
        offset += bytes_written;
        nbyte -= bytes_written;
      }
    }

    void block_log_impl::pread_with_retry(int fd, void* buf, size_t nbyte, off_t offset)
    {
      for (;;)
      {
        ssize_t bytes_read = pread(fd, buf, nbyte, offset);
        if (bytes_read == -1)
          FC_THROW("Error reading ${nbytes} from file at offset ${offset}: ${error}", 
                   ("nbyte", nbyte)("offset", offset)("error", strerror(errno)));
        if (bytes_read == (ssize_t)nbyte)
          return;
        buf = ((char*)buf) + bytes_read;
        offset += bytes_read;
        nbyte -= bytes_read;
      }
    }

    signed_block block_log_impl::read_block_from_offset_and_size(uint64_t offset, uint64_t size)
    {
      std::unique_ptr<char[]> serialized_data(new char[size]);
      pread_with_retry(block_log_fd, serialized_data.get(), size, offset);

      signed_block block;
      fc::raw::unpack_from_char_array(serialized_data.get(), size, block);
      return block;
    }

  } // end namespace detail

  block_log::block_log() : my( new detail::block_log_impl() )
  {
    my->block_log_fd = -1;
    my->block_index_fd = -1;
  }

  block_log::~block_log()
  {
    if (my->block_log_fd != -1)
      ::close(my->block_log_fd);
    if (my->block_index_fd != -1)
      ::close(my->block_index_fd);
  }

  namespace
  {
    ssize_t get_file_size(int fd)
    {
      struct stat file_stats;
      if (fstat(fd, &file_stats) == -1)
        FC_THROW("Error getting size of file: ${error}", ("error", strerror(errno)));
      return file_stats.st_size;
    }
  }

  void block_log::open( const fc::path& file )
  {
    close();

    my->block_file = file;
    my->index_file = fc::path( file.generic_string() + ".index" );

    my->block_log_fd = ::open(my->block_file.generic_string().c_str(), O_RDWR | O_APPEND | O_CREAT, 0644);
    if (my->block_log_fd == -1)
      FC_THROW("Error opening block log file ${filename}: ${error}", ("filename", my->block_file)("error", strerror(errno)));
    my->block_index_fd = ::open(my->index_file.generic_string().c_str(), O_RDWR | O_APPEND | O_CREAT, 0644);
    if (my->block_index_fd == -1)
      FC_THROW("Error opening block index file ${filename}: ${error}", ("filename", my->index_file)("error", strerror(errno)));
    my->block_log_size = get_file_size(my->block_log_fd);
    const ssize_t log_size = my->block_log_size;
    const ssize_t index_size = get_file_size(my->block_index_fd);

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
    if( log_size )
    {
      ilog( "Log is nonempty" );
      my->head.exchange(boost::make_shared<signed_block>(read_head()));

      if( index_size )
      {
        ilog( "Index is nonempty" );
        // read the last 8 bytes of the block log to get the offset of the beginning of the head 
        // block
        uint64_t block_pos;
        detail::block_log_impl::pread_with_retry(my->block_log_fd, &block_pos, sizeof(block_pos), 
                                                 log_size - sizeof(block_pos));

        // read the last 8 bytes of the block index to get the offset of the beginning of the 
        // head block
        uint64_t index_pos;
        detail::block_log_impl::pread_with_retry(my->block_index_fd, &index_pos, sizeof(index_pos), 
                                                 index_size - sizeof(index_pos));

        if( block_pos < index_pos )
        {
          ilog( "block_pos < index_pos, close and reopen index_stream" );
          ddump((block_pos)(index_pos));
          construct_index();
        }
        else if( block_pos > index_pos )
        {
          ilog( "Index is incomplete" );
          construct_index( true/*resume*/, index_pos );
        }
      }
      else
      {
        ilog( "Index is empty" );
        construct_index();
      }
    }
    else if( index_size )
    {
      ilog( "Index is nonempty, remove and recreate it" );
      if (ftruncate(my->block_index_fd, 0))
        FC_THROW("Error truncating block log: ${error}", ("error", strerror(errno)));
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
    if (my->block_log_fd != -1) {
      ::close(my->block_log_fd);
      my->block_log_fd = -1;
    }
    if (my->block_index_fd != -1) {
      ::close(my->block_index_fd);
      my->block_log_fd = -1;
    }
    my->head.store(boost::shared_ptr<signed_block>());
  }

  bool block_log::is_open()const
  {
    return my->block_log_fd != -1;
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
      uint64_t block_start_pos = my->block_log_size;
      std::vector<char> serialized_block = fc::raw::pack_to_vector(b);

      // what we write to the file is the serialized data, followed by the index of the start of the
      // serialized data.  Append that index so we can do it in a single write.
      unsigned serialized_byte_count = serialized_block.size();
      serialized_block.resize(serialized_byte_count + sizeof(uint64_t));
      *(uint64_t*)(serialized_block.data() + serialized_byte_count) = block_start_pos;

      detail::block_log_impl::write_with_retry(my->block_log_fd, serialized_block.data(), serialized_block.size());
      my->block_log_size += serialized_block.size();

      // add it to the index
      detail::block_log_impl::write_with_retry(my->block_index_fd, &block_start_pos, sizeof(block_start_pos));

      // and update our cached head block
      boost::shared_ptr<signed_block> new_head = boost::make_shared<signed_block>(b);
      my->head.exchange(new_head);

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
      boost::shared_ptr<signed_block> head_block = my->head.load();
      if (!head_block || block_num > head_block->block_num())
        return optional<signed_block>();
      if (block_num == head_block->block_num())
        return *head_block;
      // if we're still here, we know that it's in the block log, and the block after it is also
      // in the block log (which means we can determine its size)

      uint64_t offsets[2];
      uint64_t offset_in_index = sizeof(uint64_t) * (block_num - 1);
      detail::block_log_impl::pread_with_retry(my->block_index_fd, &offsets, sizeof(offsets),  offset_in_index);
      uint64_t serialized_data_size = offsets[1] - offsets[0] - sizeof(uint64_t);

      return my->read_block_from_offset_and_size(offsets[0], serialized_data_size);
    }
    FC_CAPTURE_LOG_AND_RETHROW((block_num))
  }

  vector<signed_block> block_log::read_block_range_by_num( uint32_t first_block_num, uint32_t count )const
  {
    try
    {
      vector<signed_block> result;
      uint32_t last_block_num = first_block_num + count - 1;

      // first, check if the last block we want is the current head block; if so, we can 
      // will use it and then load the previous blocks from the block log
      boost::shared_ptr<signed_block> head_block = my->head.load();
      if (!head_block || first_block_num > head_block->block_num())
        return result; // the caller is asking for blocks after the head block, we don't have them

      // if that head block will be our last block, we want it at the end of our vector,
      // so we'll tack it on at the bottom of this function
      bool last_block_is_head_block = last_block_num == head_block->block_num();
      uint32_t last_block_num_from_disk = last_block_is_head_block ? last_block_num - 1 : last_block_num;

      if (first_block_num <= last_block_num_from_disk)
      {
        // then we need to read blocks from the disk
        uint32_t number_of_blocks_to_read = last_block_num_from_disk - first_block_num + 1;
        uint32_t number_of_offsets_to_read = number_of_blocks_to_read + 1;
        // read all the offsets in one go
        std::unique_ptr<uint64_t[]> offsets(new uint64_t[number_of_blocks_to_read + 1]);
        uint64_t offset_of_first_offset = sizeof(uint64_t) * (first_block_num - 1);
        detail::block_log_impl::pread_with_retry(my->block_index_fd, offsets.get(), sizeof(uint64_t) * number_of_offsets_to_read,  offset_of_first_offset);

        // then read all the blocks in one go
        uint64_t size_of_all_blocks = offsets[number_of_blocks_to_read] - offsets[0];
	idump((size_of_all_blocks));
        std::unique_ptr<char[]> block_data(new char[size_of_all_blocks]);
        detail::block_log_impl::pread_with_retry(my->block_log_fd, block_data.get(), size_of_all_blocks,  offsets[0]);

        // now deserialize the blocks
        for (uint32_t i = 0; i <= last_block_num_from_disk - first_block_num; ++i)
        {
          uint64_t offset_in_memory = offsets[i] - offsets[0];
          uint64_t size = offsets[i + 1] - offsets[i] - sizeof(uint64_t);
          signed_block block;
          fc::raw::unpack_from_char_array(block_data.get() + offset_in_memory, size, block);
          result.push_back(std::move(block));
        }
      }

      if (last_block_is_head_block)
        result.push_back(*head_block);
      return result;
    }
    FC_CAPTURE_LOG_AND_RETHROW((first_block_num)(count))
  }

  // not thread safe, but it's only called when opening the block log, we can assume we're the only thread accessing it
  signed_block block_log::read_head()const
  {
    try
    {
      ssize_t block_log_size = get_file_size(my->block_log_fd);

      // read the last int64 of the block log into `head_block_offset`, 
      // that's the index of the start of the head block
      FC_ASSERT(block_log_size >= (ssize_t)sizeof(uint64_t));
      uint64_t head_block_offset;
      detail::block_log_impl::pread_with_retry(my->block_log_fd, &head_block_offset, sizeof(head_block_offset), 
                                               block_log_size - sizeof(head_block_offset));

      return my->read_block_from_offset_and_size(head_block_offset, block_log_size - head_block_offset - sizeof(head_block_offset));
    }
    FC_LOG_AND_RETHROW()
  }

  const boost::shared_ptr<signed_block> block_log::head()const
  {
    return my->head.load();
  }

  void block_log::construct_index( bool resume, uint64_t index_pos )
  {
    try
    {
      // Note: the old implementation recreated the block index by reading the log
      // forwards, starting at the start of the block log and deserializing each block
      // and then writing the new file position to the index.
      // The new implementation recreates the index by reading the block log backwards,
      // extracting only the offsets from the block log.  This should be much more efficient 
      // when regenerating the whole index, but it lacks the ability to repair the end
      // of the index.  
      // It would probably be worthwhile to use the old method if the number of
      // entries to repair is small compared to the total size of the block log
      ilog( "Reconstructing Block Log Index..." );

      // the caller has already loaded the head block
      boost::shared_ptr<signed_block> head_block = my->head.load();
      FC_ASSERT(head_block);

      fc::path new_index_file(my->index_file.generic_string() + ".new");
      int new_index_fd = ::open(new_index_file.generic_string().c_str(), O_RDWR | O_CREAT | O_TRUNC, 0644);
      if (my->block_index_fd == -1)
        FC_THROW("Error opening temporary new index file ${filename}: ${error}", ("filename", new_index_file.generic_string())("error", strerror(errno)));

      uint32_t block_num = head_block->block_num();
      idump((block_num));
      if (ftruncate(new_index_fd, sizeof(uint64_t) * block_num) == -1)
        FC_THROW("Error resizing rebuilt block index file: ${error}", ("error", strerror(errno)));

      uint64_t block_log_offset_of_block_pos = my->block_log_size - sizeof(uint64_t);

      // now walk backwards through the block log reading the starting positions of the blocks
      // and writing them to the index
      while (!appbase::app().is_interrupt_request() && block_num)
      {
        // read the file offset of the start of the block from the block log
        uint64_t block_pos;
        detail::block_log_impl::pread_with_retry(my->block_log_fd, &block_pos, sizeof(block_pos), 
                                                 block_log_offset_of_block_pos);

        // write it to the right location in the index
        detail::block_log_impl::pwrite_with_retry(new_index_fd, &block_pos, sizeof(block_pos), sizeof(block_pos) * (block_num - 1));

        --block_num;
        block_log_offset_of_block_pos = block_pos - sizeof(uint64_t);
      }
      if( appbase::app().is_interrupt_request() )
        ilog("Creating Block Log Index is interrupted on user request. Last applied: (block number: ${n})",
             ("n", block_num));
      else
      {
        ::close(new_index_fd);
        ::close(my->block_index_fd);
        fc::remove_all(my->index_file);
        fc::rename(new_index_file, my->index_file);
        my->block_index_fd = ::open(my->index_file.generic_string().c_str(), O_RDWR | O_APPEND | O_CREAT, 0644);
        if (my->block_index_fd == -1)
          FC_THROW("Error opening block index file ${filename}: ${error}", ("filename", my->index_file)("error", strerror(errno)));
      }
#if 0
      ilog( "Reconstructing Block Log Index..." );
      my->index_stream.close();

      if( !resume )
        fc::remove_all( my->index_file );

      my->index_stream.open( my->index_file.generic_string().c_str(), LOG_WRITE );
      my->index_write = true;

      uint64_t pos = resume ? index_pos : 0;
      uint64_t end_pos;
      my->check_block_read();

      my->block_stream.seekg( -sizeof( uint64_t), std::ios::end );
      my->block_stream.read( (char*)&end_pos, sizeof( end_pos ) );
      signed_block tmp;

      my->block_stream.seekg( pos );
      if( resume )
      {
        my->index_stream.seekg( 0, std::ios::end );

        fc::raw::unpack( my->block_stream, tmp );
        my->block_stream.read( (char*)&pos, sizeof( pos ) );

        ilog("Resuming Block Log Index. Last applied: ( block number: ${n} )( trx: ${trx} )( bytes position: ${pos} )",
                                                            ( "n", tmp.block_num() )( "trx", tmp.id() )( "pos", pos ) );
      }

      while( !appbase::app().is_interrupt_request() && pos < end_pos )
      {
        fc::raw::unpack( my->block_stream, tmp );
        my->block_stream.read( (char*)&pos, sizeof( pos ) );
        my->index_stream.write( (char*)&pos, sizeof( pos ) );
      }

      if( appbase::app().is_interrupt_request() )
        ilog("Creating Block Log Index is interrupted on user request. Last applied: ( block number: ${n} )( trx: ${trx} )( bytes position: ${pos} )",
                                                            ( "n", tmp.block_num() )( "trx", tmp.id() )( "pos", pos ) );

      /// Flush and reopen to be sure that given index file has been saved.
      /// Otherwise just executed replay, next stopped by ctrl+C can again corrupt this file. 
      my->index_stream.flush();
      my->index_stream.close();
      my->index_stream.open(my->index_file.generic_string().c_str(), LOG_WRITE);
#endif
    }
    FC_LOG_AND_RETHROW()
  }

} } // hive::chain
