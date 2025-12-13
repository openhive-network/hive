#pragma once

#include <memory>

namespace boost { namespace signals2 {
  class connection;
} }

namespace hive { namespace utilities {

struct signal_connection_deleter
{
  void operator()( boost::signals2::connection* ptr ) const;
};

using signal_connection_ptr = std::unique_ptr<boost::signals2::connection, signal_connection_deleter>;

/// Creates a signal_connection_ptr from a connection object (must be called where connection is complete type)
signal_connection_ptr make_signal_connection_ptr( boost::signals2::connection&& conn );

} }
