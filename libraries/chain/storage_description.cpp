
#include <hive/chain/storage_description.hpp>

#include <hive/chain/file_operation.hpp>

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

namespace hive { namespace chain {

  storage_description::storage_description( storage_description::storage_type val, const std::string& file_name_ext_val )
                      : storage( val ), file_name_ext( file_name_ext_val )
  {
  }

  void storage_description::calculate_status( uint64_t block_pos )
  {
    if( block_pos < pos )
      status = storage_description::status_type::reopen;
    else if( block_pos > pos )
    {
      status = storage_description::status_type::resume;
      diff  = block_pos - pos;
    }
    else
      status = storage_description::status_type::none;
  }

  void storage_description::check_consistency( uint32_t element_size, uint32_t total_size )
  {
    //report size of new index file and verify it is the right size for the blocks in block log
    struct stat block_index_stat;
    if (fstat( file_descriptor, &block_index_stat ) == -1)
      elog("error: could not get size of file: ${file}",("file",file));
    idump((block_index_stat.st_size));
    FC_ASSERT(block_index_stat.st_size/element_size == total_size);
  }

  void storage_description::open( const fc::path& input_file )
  {
    file = fc::path( input_file.generic_string() + file_name_ext );
    open();
  }

  void storage_description::open()
  {
    FC_ASSERT( file.string().size(), "unknown file name" );

    file_descriptor = ::open(file.generic_string().c_str(), O_RDWR | O_APPEND | O_CREAT | O_CLOEXEC, 0644);
    if( file_descriptor == -1 )
      FC_THROW("Error opening afile ${filename}: ${error}", ("filename", file)("error", strerror(errno)));

    size = file_operation::get_file_size( file_descriptor );
  }

  void storage_description::close()
  {
    if( file_descriptor != -1 )
    {
      ::close(file_descriptor);
      file_descriptor = -1;
    }

    status  = status_type::none;
    size    = 0;
    pos     = 0;
  }

  bool storage_description::is_open() const
  {
    return file_descriptor != -1;
  }

} } // hive::chain
