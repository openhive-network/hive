#pragma once

#include <fc/variant_object.hpp>

namespace hive { namespace protocol {

fc::variant_object get_config( const std::string& treasury_name );

} } // hive::protocol
