#pragma once

#include <chainbase/pool_allocator.hpp>

#include <boost/interprocess/sync/interprocess_sharable_mutex.hpp>
#include <boost/interprocess/sync/sharable_lock.hpp>
#include <boost/interprocess/sync/file_lock.hpp>

#include <boost/thread.hpp>
#include <boost/thread/locks.hpp>

namespace chainbase {

  using namespace boost::multi_index;

  typedef boost::shared_mutex read_write_mutex;
  typedef boost::shared_lock<read_write_mutex> read_lock;
  typedef boost::unique_lock<read_write_mutex> write_lock;

}
