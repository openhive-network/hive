#include <hive/utilities/signal_connection_ptr.hpp>

#include <boost/signals2/connection.hpp>

namespace hive { namespace utilities {

void signal_connection_deleter::operator()( boost::signals2::connection* ptr ) const
{
  delete ptr;
}

signal_connection_ptr make_signal_connection_ptr( boost::signals2::connection&& conn )
{
  return signal_connection_ptr( new boost::signals2::connection( std::move( conn ) ) );
}

} } // hive::utilities
