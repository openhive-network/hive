#include <hive/chain/block_log.hpp>
#include <hive/chain/file_operation.hpp>
#include <hive/chain/file_manager.hpp>

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
      private:

        file_manager mgr;

      public:

        void open( const fc::path& file )
        {
          mgr.prepare( file );
        }

        void close()
        {
          mgr.close();
        }

        uint64_t append( const signed_block& b )
        {
          try
          {
            return mgr.append( b );
          }
          FC_LOG_AND_RETHROW()
        }

        // threading guarantees:
        // no restrictions
        void flush()
        {
          // writes to file descriptors are flushed automatically
        }

        optional< signed_block > read_block_by_num( uint32_t block_num )
        {
          try
          {
            return mgr.read_block_by_num( block_num );
          }
          FC_CAPTURE_LOG_AND_RETHROW((block_num))
        }

        vector<signed_block> read_block_range_by_num( uint32_t first_block_num, uint32_t count )
        {
          try
          {
            return mgr.get_block_log_idx()->read_block_range( first_block_num, count, mgr.get_block_log_file() );
          }
          FC_CAPTURE_LOG_AND_RETHROW((first_block_num)(count))
        }

        std::tuple< optional<block_id_type>, optional<public_key_type> > read_data_by_num( uint32_t block_num )
        {
          try
          {
            return mgr.get_hash_idx()->read_data_by_num( block_num );
          }
          FC_CAPTURE_LOG_AND_RETHROW((block_num))
        }

        std::map< uint32_t, std::tuple< optional<block_id_type>, optional<public_key_type> > > read_data_range_by_num( uint32_t first_block_num, uint32_t count )
        {
          try
          {
            return mgr.get_hash_idx()->read_data_range_by_num( first_block_num, count );
          }
          FC_CAPTURE_LOG_AND_RETHROW((first_block_num)(count))
        }

        // not thread safe, but it's only called when opening the block log, we can assume we're the only thread accessing it
        signed_block read_head()
        {
          return mgr.read_head();
        }

        const boost::shared_ptr<signed_block> head()
        {
          return mgr.get_block_log_file().head.load();
        }
    };

  } // end namespace detail

  block_log::block_log() : my( new detail::block_log_impl() )
  {
  }

  block_log::~block_log()
  {
  }

  void block_log::open( const fc::path& file )
  {
    my->open( file );
  }

  void block_log::close()
  {
    my->close();
  }

  uint64_t block_log::append( const signed_block& b )
  {
    return my->append( b );
  }

  // threading guarantees:
  // no restrictions
  void block_log::flush()
  {
    // writes to file descriptors are flushed automatically
  }

  optional< signed_block > block_log::read_block_by_num( uint32_t block_num )const
  {
    return my->read_block_by_num( block_num );
  }

  vector<signed_block> block_log::read_block_range_by_num( uint32_t first_block_num, uint32_t count )const
  {
    return my->read_block_range_by_num( first_block_num, count );
  }

  std::tuple< optional<block_id_type>, optional<public_key_type> > block_log::read_data_by_num( uint32_t block_num )
  {
    return my->read_data_by_num( block_num );
  }

  std::map< uint32_t, std::tuple< optional<block_id_type>, optional<public_key_type> > > block_log::read_data_range_by_num( uint32_t first_block_num, uint32_t count )
  {
    return my->read_data_range_by_num( first_block_num, count );
  }

  signed_block block_log::read_head()const
  {
    return my->read_head();
  }

  const boost::shared_ptr<signed_block> block_log::head()const
  {
    return my->head();
  }

} } // hive::chain
