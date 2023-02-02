#include <hive/protocol/fixed_string.hpp>

namespace hive { namespace protocol { namespace details {

  thread_local bool truncation_controller::verify = true;

} } }
