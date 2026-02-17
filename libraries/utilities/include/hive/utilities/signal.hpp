#pragma once

#include <hive/utilities/signal_connection_ptr.hpp>

namespace hive { namespace utilities {

/// Disconnects a signal connection and asserts it is disconnected.
void disconnect_signal( boost::signals2::connection& signal );

/// Disconnects and resets an opaque signal_connection_ptr.
void disconnect_signal( signal_connection_ptr& signal );

} }
