#pragma once

#include <hive/utilities/signal_connection_ptr.hpp>
#include <fc/signals.hpp>

namespace hive { namespace utilities {

inline void disconnect_signal( boost::signals2::connection& signal )
{
  if( signal.connected() )
    signal.disconnect();
  FC_ASSERT( !signal.connected() );
}

inline void disconnect_signal( signal_connection_ptr& signal )
{
  if( signal )
  {
    disconnect_signal( *signal );
    signal.reset();
  }
}

} }
