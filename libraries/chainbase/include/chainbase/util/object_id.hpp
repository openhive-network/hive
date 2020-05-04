#pragma once

#include <cstdint>
#include <cstdlib>

namespace chainbase
{

/**
*  Object ID type that includes the type of the object it references
*/
template<typename T>
class oid
{

using __id_type = uint32_t;

public:
   oid( __id_type i = 0 ):_id(i){}

   oid& operator++() { ++_id; return *this; }

   operator size_t () const
   {
      return static_cast<size_t>(_id);
   }

   friend bool operator < ( const oid& a, const oid& b ) { return a._id < b._id; }
   friend bool operator > ( const oid& a, const oid& b ) { return a._id > b._id; }
   friend bool operator == ( const oid& a, const oid& b ) { return a._id == b._id; }
   friend bool operator != ( const oid& a, const oid& b ) { return a._id != b._id; }
   __id_type _id = 0;
};

} /// namespace chainbase
