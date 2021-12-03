#include <hive/chain/file_manager.hpp>

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

  file_manager::file_manager()
  {
    idxs.push_back( p_base_index( new block_log_index( storage_description::storage_type::block_log_idx, ".index" ) ) );
    //idxs.push_back( p_base_index( new block_log_index( storage_description::storage_type::hash_idx, "_hash.index" ) ) );
  }

  file_manager::~file_manager()
  {
  }

  void file_manager::close()
  {
    block_log.close();

    for( auto& idx : idxs )
      idx->close();
  }

  block_log_file& file_manager::get_block_log_file()
  {
    return block_log;
  }

  file_manager::p_base_index& file_manager::get_block_log_idx()
  {
    FC_ASSERT( BLOCK_LOG_IDX < idxs.size(), "lack of block_log index");
    return idxs[BLOCK_LOG_IDX];
  }

  file_manager::p_base_index& file_manager::get_hash_idx()
  {
    FC_ASSERT( HASH_IDX < idxs.size(), "lack of hash index");
    return idxs[HASH_IDX];
  }

  bool file_manager::get_resume() const
  {
    size_t cnt = 0;

    for( auto& idx : idxs )
      if( idx->storage.status == storage_description::status_type::resume )
        ++cnt;

    return cnt == idxs.size();
  }

  uint64_t file_manager::get_index_pos() const
  {
    std::set<uint64_t> res;

    for( auto& idx : idxs )
      res.insert( idx->storage.pos );

    if( res.size() == 1 )
      return *res.begin();
    else
      return 0;
  }

  void file_manager::construct_index()
  {
    try
    {
      boost::shared_ptr<signed_block> head_block  = block_log.head.load();
      auto& block_chain_storage                   = block_log.storage;
      bool resume                                 = get_resume();
      uint64_t index_pos                          = get_index_pos();

      FC_ASSERT(head_block);
      const uint32_t block_num = head_block->block_num();
      idump((block_num));

//#define USE_BACKWARDS_INDEX
      // Note: using backwards indexing has to be used when `block_api` plugin is enabled,
      // because this plugin retrieves [block hash;witness public key] from `hash.index` 
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
      char* block_log_ptr = (char*)mmap(0, block_chain_storage.size, PROT_READ, MAP_SHARED, block_chain_storage.file_descriptor, 0);
      if (block_log_ptr == (char*)-1)
        FC_THROW("Failed to mmap block log file: ${error}",("error",strerror(errno)));
      if (madvise(block_log_ptr, block_chain_storage.size, MADV_WILLNEED) == -1)
        wlog("madvise failed: ${error}",("error",strerror(errno)));

      //memory map for index file
      char* block_index_ptr = (char*)mmap(0, block_index_size, PROT_WRITE, MAP_SHARED, new_index_fd, 0);
      if (block_index_ptr == (char*)-1)
        FC_THROW("Failed to mmap block log index: ${error}",("error",strerror(errno)));
#endif
      // now walk backwards through the block log reading the starting positions of the blocks
      // and writing them to the index
      uint32_t block_index = block_num;
      uint64_t block_log_offset_of_block_pos = block_chain_storage.size - sizeof(uint64_t);
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
        file_operation::pread_with_retry(block_chain_storage.file_descriptor, &block_pos, sizeof(block_pos), block_log_offset_of_block_pos);
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
      if (munmap(block_log_ptr, block_chain_storage.size) == -1)
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

      std::vector<std::fstream> index_streams( idxs.size() );

      if( !resume )
        for( auto& idx : idxs )
          fc::remove_all( idx->storage.file );

      block_stream.open( block_chain_storage.file.generic_string().c_str(), LOG_READ );

      uint32_t cnt = 0;
      for( auto& idx : idxs )
        index_streams[ cnt++ ].open( idx->storage.file.generic_string().c_str(), LOG_WRITE );

      uint64_t pos = resume ? index_pos : 0;
      uint64_t end_pos;

      block_stream.seekg( -sizeof( uint64_t), std::ios::end );
      block_stream.read( (char*)&end_pos, sizeof( end_pos ) );
      signed_block tmp;

      block_stream.seekg( pos );
      if( resume )
      {
        for( auto& index_stream : index_streams )
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

        for( auto& index_stream : index_streams )
          index_stream.write( (char*)&pos, sizeof( pos ) );
      }

      if( appbase::app().is_interrupt_request() )
        ilog("Creating Block Log Index is interrupted on user request. Last applied: ( block number: ${n} )( trx: ${trx} )( bytes position: ${pos} )",
                                                            ( "n", tmp.block_num() )( "trx", tmp.id() )( "pos", pos ) );

      /// Flush and reopen to be sure that given index file has been saved.
      /// Otherwise just executed replay, next stopped by ctrl+C can again corrupt this file. 
      for( auto& index_stream : index_streams )
        index_stream.close();

      for( auto& idx : idxs )
        idx->close();

#endif //NOT USE_BACKWARD_INDEX

      ilog("opening new ${info}", ("info", ( idxs.size() == 1 ) ? "block index" : "indexes"));
      for( auto& idx : idxs )
      {
        idx->open();
        idx->check_consistency( block_num );
      }
    }
    FC_LOG_AND_RETHROW()
  }

} } // hive::chain
