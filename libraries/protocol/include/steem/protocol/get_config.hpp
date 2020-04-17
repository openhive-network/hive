#pragma once

#include <fc/variant_object.hpp>

namespace steem { namespace protocol {

fc::variant_object get_config( const std::string& treasury_name );

} } // steem::protocol
