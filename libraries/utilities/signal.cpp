#include <hive/utilities/signal.hpp>

#include <boost/signals2/connection.hpp>
#include <fc/exception/exception.hpp>

namespace hive { namespace utilities {

void disconnect_signal( boost::signals2::connection& signal )
{
  if( signal.connected() )
    signal.disconnect();
  FC_ASSERT( !signal.connected() );
}

void disconnect_signal( signal_connection_ptr& signal )
{
  if( signal )
  {
    disconnect_signal( *signal );
    signal.reset();
  }
}

} } // hive::utilities
