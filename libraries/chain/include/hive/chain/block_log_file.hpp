#pragma once

#include <hive/chain/storage_description.hpp>

#include <hive/protocol/block.hpp>

#include <boost/smart_ptr/atomic_shared_ptr.hpp>

namespace hive { namespace chain {

  using namespace hive::protocol;

  class block_log_file
  {
    public:

      boost::atomic_shared_ptr<signed_block> head;

      storage_description storage;

      block_log_file();

      void open( const fc::path& file );
      void close();
  };

} } // hive::chain
