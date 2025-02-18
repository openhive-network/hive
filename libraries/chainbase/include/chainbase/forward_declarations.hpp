#pragma once

// This header forward-declares pack/unpack and to/from variant functions.
// These declarations need to be as early as possible to prevent compiler errors.

#include <chainbase/allocators.hpp>
#include <chainbase/util/object_id.hpp>
#include <chainbase/util/object_id_serialization.hpp>

#include <fc/io/raw_constants.hpp>

#include <optional>

namespace fc {

namespace raw {

#ifndef ENABLE_STD_ALLOCATOR
template<typename Stream>
inline void pack( Stream& s, const chainbase::shared_string& ss );
template<typename Stream>
inline void unpack( Stream& s, chainbase::shared_string& ss, uint32_t depth = 0, bool limit_is_disabled = false, const uint32_t max_depth = MAX_RECURSION_DEPTH );
#endif

template<typename Stream, typename E, typename A>
void pack( Stream& s, const boost::interprocess::deque< E, A >& value );
template<typename Stream, typename E, typename A>
void unpack( Stream& s, boost::interprocess::deque< E, A >& value, uint32_t depth = 0, bool limit_is_disabled = false, const uint32_t max_depth = MAX_RECURSION_DEPTH );

template<typename Stream, typename K, typename V, typename C, typename A>
void pack( Stream& s, const boost::interprocess::flat_map< K, V, C, A >& value );
template<typename Stream, typename K, typename V, typename C, typename A>
void unpack( Stream& s, boost::interprocess::flat_map< K, V, C, A >& value, uint32_t depth = 0, bool limit_is_disabled = false, const uint32_t max_depth = MAX_RECURSION_DEPTH );

/*
inline void pack_to_slice
inlince void unpack_from_slice
*/

/*
account_name_type (fixed_string)
transaction_id_type

*/

} }
