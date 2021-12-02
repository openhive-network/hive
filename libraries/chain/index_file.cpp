#include <hive/chain/index_file.hpp>
#include <hive/chain/file_operation.hpp>

#include <appbase/application.hpp>

#include <fstream>

#define MMAP_BLOCK_IO

#ifdef MMAP_BLOCK_IO
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#endif

#define LOG_READ  (std::ios::in | std::ios::binary)
#define LOG_WRITE (std::ios::out | std::ios::binary | std::ios::app)

namespace hive { namespace chain {

  block_log_index::block_log_index()
  {
  }

  block_log_index::~block_log_index()
  {
    close();
  }

  int block_log_index::read_blocks_number( const ssize_t index_size, uint64_t block_pos )
  {
    // read the last 8 bytes of the block index to get the offset of the beginning of the 
    // head block
    uint64_t index_pos = 0;
    size_t bytes_read = file_operation::pread_with_retry( storage.file_descriptor, &index_pos, sizeof(index_pos), 
      index_size - sizeof(index_pos));

    FC_ASSERT(bytes_read == sizeof(index_pos));

    if( block_pos < index_pos )
      return 1;
    else if( block_pos > index_pos )
      return -1;

    return 0;
  }

  ssize_t block_log_index::open( const fc::path& file )
  {
    storage.file = fc::path( file.generic_string() + ".index" );

    storage.file_descriptor = ::open( storage.file.generic_string().c_str(), O_RDWR | O_APPEND | O_CREAT | O_CLOEXEC, 0644 );
    if( storage.file_descriptor == -1 )
      FC_THROW("Error opening block index file ${filename}: ${error}", ("filename", storage.file)("error", strerror(errno)));

    return file_operation::get_file_size( storage.file_descriptor );
  }

  void block_log_index::prepare( const ssize_t index_size, const boost::shared_ptr<signed_block>& head_block, const storage_description_ex& desc )
  {
    if( index_size )
    {
      ilog( "Index is nonempty" );
      // read the last 8 bytes of the block log to get the offset of the beginning of the head 
      // block
      uint64_t block_pos = 0;
      auto bytes_read = file_operation::pread_with_retry(desc.file_descriptor, &block_pos, sizeof(block_pos), 
        desc.block_log_size - sizeof(block_pos));

      FC_ASSERT(bytes_read == sizeof(block_pos));

      int cmp = read_blocks_number( index_size, block_pos );
      // read the last 8 bytes of the block index to get the offset of the beginning of the 
      // head block
      uint64_t index_pos = 0;
      bytes_read = file_operation::pread_with_retry( storage.file_descriptor, &index_pos, sizeof(index_pos), 
        index_size - sizeof(index_pos));

      FC_ASSERT(bytes_read == sizeof(index_pos));

      if( cmp == 1 )
      {
        ilog( "block_pos < index_pos, close and reopen index_stream" );
        ddump((block_pos)(index_pos));
        construct_index( head_block, desc );
      }
      else if( cmp == -1 )
      {
        ilog( "Index is incomplete" );
        construct_index( head_block, desc, true/*resume*/, index_pos );
      }
    }
    else
    {
      ilog( "Index is empty" );
      construct_index( head_block, desc );
    }
  }

  void block_log_index::close()
  {
    if( storage.file_descriptor != -1 )
    {
      ::close( storage.file_descriptor );
      storage.file_descriptor = -1;
    }
  }

  void block_log_index::construct_index( const boost::shared_ptr<signed_block>& head_block, const storage_description_ex& desc, bool resume, uint64_t index_pos )
  {
    try
    {
      FC_ASSERT(head_block);
      const uint32_t block_num = head_block->block_num();
      idump((block_num));

//#define USE_BACKWARDS_INDEX
      // Note: using backwards indexing has to be used when `block_api` plugin is enabled,
      // because this plugin retrieves [block hash;witness public key] from `hashes.index` 
      // when `block_api.get_block` or `block_api.get_block_range` is called

#ifdef USE_BACKWARDS_INDEX
      // Note: the old implementation recreated the block index by reading the log
      // forwards, starting at the start of the block log and deserializing each block
      // and then writing the new file position to the index.
      // The new implementation recreates the index by reading the block log backwards,
      // extracting only the offsets from the block log.  This should be much more efficient 
      // when regenerating the whole index, but it lacks the ability to repair the end
      // of the index.  
      // It would probably be worthwhile to use the old method if the number of
      // entries to repair is small compared to the total size of the block log
#ifdef MMAP_BLOCK_IO      
      ilog( "Reconstructing Block Log Index using memory-mapped IO...resume=${resume},index_pos=${index_pos}",("resume",resume)("index_pos",index_pos) );
#else
      ilog( "Reconstructing Block Log Index...resume=${resume},index_pos=${index_pos}",("resume",resume)("index_pos",index_pos) );
#endif
      //close old index file if open, we'll reopen after we replace it
      ::close( storage.file_descriptor );

      //create and size the new temporary index file (block_log.index.new)
      fc::path new_index_file( storage.file.generic_string() + ".new" );
      const size_t block_index_size = block_num * sizeof(uint64_t);
      int new_index_fd = ::open(new_index_file.generic_string().c_str(), O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
      if (new_index_fd == -1)
        FC_THROW("Error opening temporary new index file ${filename}: ${error}", ("filename", new_index_file.generic_string())("error", strerror(errno)));
      if (ftruncate(new_index_fd, block_index_size) == -1)
        FC_THROW("Error resizing rebuilt block index file: ${error}", ("error", strerror(errno)));

#ifdef MMAP_BLOCK_IO
      //memory map for block log
      char* block_log_ptr = (char*)mmap(0, desc.block_log_size, PROT_READ, MAP_SHARED, desc.file_descriptor, 0);
      if (block_log_ptr == (char*)-1)
        FC_THROW("Failed to mmap block log file: ${error}",("error",strerror(errno)));
      if (madvise(block_log_ptr, desc.block_log_size, MADV_WILLNEED) == -1)
        wlog("madvise failed: ${error}",("error",strerror(errno)));

      //memory map for index file
      char* block_index_ptr = (char*)mmap(0, block_index_size, PROT_WRITE, MAP_SHARED, new_index_fd, 0);
      if (block_index_ptr == (char*)-1)
        FC_THROW("Failed to mmap block log index: ${error}",("error",strerror(errno)));
#endif
      // now walk backwards through the block log reading the starting positions of the blocks
      // and writing them to the index
      uint32_t block_index = block_num;
      uint64_t block_log_offset_of_block_pos = desc.block_log_size - sizeof(uint64_t);
      while (!appbase::app().is_interrupt_request() && block_index)
      {
        // read the file offset of the start of the block from the block log
        uint64_t block_pos;
        uint64_t higher_block_pos;
        higher_block_pos = block_log_offset_of_block_pos;
#ifdef MMAP_BLOCK_IO
        //read next block pos offset from the block log
        memcpy(&block_pos, block_log_ptr + block_log_offset_of_block_pos, sizeof(block_pos));
        // write it to the right location in the new index file
        memcpy(block_index_ptr + sizeof(block_pos) * (block_index - 1), &block_pos, sizeof(block_pos));
#else
        //read next block pos offset from the block log
        file_operation::pread_with_retry(desc.file_descriptor, &block_pos, sizeof(block_pos), block_log_offset_of_block_pos);
        // write it to the right location in the new index file
        file_operation::pwrite_with_retry(new_index_fd, &block_pos, sizeof(block_pos), sizeof(block_pos) * (block_index - 1));
#endif
        if (higher_block_pos <= block_pos) //this is a sanity check on index values stored in the block log
          FC_THROW("bad block index at block ${block_index} because ${higher_block_pos} <= ${block_pos}",
                  ("block_index",block_index)("higher_block_pos",higher_block_pos)("block_pos",block_pos));

        --block_index;
        block_log_offset_of_block_pos = block_pos - sizeof(uint64_t);
      } //while writing block index
      if (appbase::app().is_interrupt_request())
      {
        ilog("Creating Block Log Index was interrupted on user request and can't be resumed. Last applied: (block number: ${n})", ("n", block_num));
        return;
      }

#ifdef MMAP_BLOCK_IO
      if (munmap(block_log_ptr, desc.block_log_size) == -1)
        elog("error unmapping block_log: ${error}",("error",strerror(errno)));
      if (munmap(block_index_ptr, block_index_size) == -1)
        elog("error unmapping block_index: ${error}",("error",strerror(errno)));
#endif  
      ::close(new_index_fd);
      fc::remove_all( storage.file );
      fc::rename( new_index_file, storage.file);
#else //NOT USE_BACKWARDS_INDEX
      ilog( "Reconstructing Block Log Index in forward direction (old slower way, but allows for interruption)..." );
      std::fstream block_stream;
      std::fstream index_stream;

      if( !resume )
        fc::remove_all( storage.file );

      block_stream.open( desc.file.generic_string().c_str(), LOG_READ );
      index_stream.open( storage.file.generic_string().c_str(), LOG_WRITE );

      uint64_t pos = resume ? index_pos : 0;
      uint64_t end_pos;

      block_stream.seekg( -sizeof( uint64_t), std::ios::end );      
      block_stream.read( (char*)&end_pos, sizeof( end_pos ) );      
      signed_block tmp;

      block_stream.seekg( pos );
      if( resume )
      {
        index_stream.seekg( 0, std::ios::end );

        fc::raw::unpack( block_stream, tmp );
        block_stream.read( (char*)&pos, sizeof( pos ) );

        ilog("Resuming Block Log Index. Last applied: ( block number: ${n} )( trx: ${trx} )( bytes position: ${pos} )",
                                                            ( "n", tmp.block_num() )( "trx", tmp.id() )( "pos", pos ) );
      }

      while( !appbase::app().is_interrupt_request() && pos < end_pos )
      {
        fc::raw::unpack( block_stream, tmp );
        block_stream.read( (char*)&pos, sizeof( pos ) );
        index_stream.write( (char*)&pos, sizeof( pos ) );
      }

      if( appbase::app().is_interrupt_request() )
        ilog("Creating Block Log Index is interrupted on user request. Last applied: ( block number: ${n} )( trx: ${trx} )( bytes position: ${pos} )",
                                                            ( "n", tmp.block_num() )( "trx", tmp.id() )( "pos", pos ) );

      /// Flush and reopen to be sure that given index file has been saved.
      /// Otherwise just executed replay, next stopped by ctrl+C can again corrupt this file. 
      index_stream.close();
      ::close( storage.file_descriptor );
#endif //NOT USE_BACKWARD_INDEX

      ilog("opening new block index");
      storage.file_descriptor = ::open( storage.file.generic_string().c_str(), O_RDWR | O_APPEND | O_CREAT | O_CLOEXEC, 0644 );
      if (storage.file_descriptor == -1)
        FC_THROW("Error opening block index file ${filename}: ${error}", ("filename", storage.file)("error", strerror(errno)));
      //report size of new index file and verify it is the right size for the blocks in block log
      struct stat block_index_stat;
      if (fstat( storage.file_descriptor, &block_index_stat ) == -1)
        elog("error: could not get size of block log index");
      idump((block_index_stat.st_size));
      FC_ASSERT(block_index_stat.st_size/sizeof(uint64_t) == block_num);
    }
    FC_LOG_AND_RETHROW()
  }

  void block_log_index::non_empty_idx_info()
  {
    ilog( "Index is nonempty, remove and recreate it" );
    if (ftruncate( storage.file_descriptor, 0 ))
      FC_THROW("Error truncating block log: ${error}", ("error", strerror(errno)));
  }

  void block_log_index::append( const void* buf, size_t nbyte )
  {
    file_operation::write_with_retry( storage.file_descriptor, buf, nbyte );
  }

  void block_log_index::read( uint32_t block_num, uint64_t& offset, uint64_t& size )
  {
    uint64_t offsets[2] = {0, 0};
    uint64_t offset_in_index = sizeof(uint64_t) * (block_num - 1);
    auto bytes_read = file_operation::pread_with_retry( storage.file_descriptor, &offsets, sizeof(offsets),  offset_in_index );
    FC_ASSERT(bytes_read == sizeof(offsets));

    offset = offsets[0];
    size = offsets[1] - offsets[0] - sizeof(uint64_t);
  }

  vector<signed_block> block_log_index::read_block_range( uint32_t first_block_num, uint32_t count, int block_log_fd, const boost::shared_ptr<signed_block>& head_block )
  {

    vector<signed_block> result;

    uint32_t last_block_num = first_block_num + count - 1;

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
      file_operation::pread_with_retry( storage.file_descriptor, offsets.get(), sizeof(uint64_t) * number_of_offsets_to_read,  offset_of_first_offset);

      // then read all the blocks in one go
      uint64_t size_of_all_blocks = offsets[number_of_blocks_to_read] - offsets[0];
      idump((size_of_all_blocks));
      std::unique_ptr<char[]> block_data(new char[size_of_all_blocks]);
      file_operation::pread_with_retry(storage.file_descriptor, block_data.get(), size_of_all_blocks,  offsets[0]);

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

} } // hive::chain
