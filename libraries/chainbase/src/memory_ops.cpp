#include <chainbase/memory_ops.hpp>

namespace chainbase {

// Initialize static member variables
long vm_dirty_params::dirty_bytes = 0;
long vm_dirty_params::dirty_background_bytes = 0;
long vm_dirty_params::dirty_expire_centisecs = 0;
long vm_dirty_params::swappiness = 0;
long vm_dirty_params::dirty_ratio = 0;
long vm_dirty_params::dirty_background_ratio = 0;
bool vm_dirty_params::values_saved = false;

} // namespace chainbase
