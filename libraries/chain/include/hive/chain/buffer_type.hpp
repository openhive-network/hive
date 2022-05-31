#pragma once

#include <hive/protocol/buffer_type_pack.hpp>

#include <chainbase/chainbase.hpp>

#include <fc/io/datastream.hpp>
#include <fc/io/raw.hpp>

namespace hive { namespace chain {

typedef chainbase::t_vector< char > buffer_type;

} } // hive::chain

#ifndef ENABLE_STD_ALLOCATOR
FC_REFLECT_TYPENAME( hive::chain::buffer_type )
#endif