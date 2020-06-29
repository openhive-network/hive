#pragma once

#include <fc/variant_object.hpp>
#include <fc/crypto/sha256.hpp>
#include <hive/protocol/config.hpp>

namespace hive { namespace protocol {

fc::variant_object get_config( const config_blockchain_type& config, const std::string& treasury_name );

} } // hive::protocol
