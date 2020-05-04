#pragma once

#include <fc/variant_object.hpp>
#include <fc/crypto/sha256.hpp>

namespace hive { namespace protocol {

fc::variant_object get_config( const std::string& treasury_name, const fc::sha256& chain_id );

} } // hive::protocol
