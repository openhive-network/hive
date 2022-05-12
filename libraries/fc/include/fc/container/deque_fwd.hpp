#pragma once

#include <fc/io/pack_flags.hpp>
#include <deque>

namespace fc {

   namespace raw {
       template<typename Stream, typename T>
       void pack( Stream& s, const std::deque<T>& value, const pack_flags& flags );
       template<typename Stream, typename T>
       void unpack( Stream& s, std::deque<T>& value, uint32_t depth = 0 );
   }
} // namespace fc
