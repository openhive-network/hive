#pragma once

#include <memory>

namespace boost { namespace signals2 {
  class connection;
} }

namespace hive { namespace utilities {

using signal_connection_ptr = std::unique_ptr<boost::signals2::connection>;

} }
