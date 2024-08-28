#pragma once

#include <chainbase/util/object_id.hpp>
#include <fc/reflect/reflect.hpp>
#include <fc/reflect/variant.hpp>

namespace fc
{
class variant;

template<typename... T> struct get_typename;

template<typename T>
void to_variant(const chainbase::oid<T>& var, variant& vo)
{
  vo = var.get_value();
}

template<typename T>
void to_variant(const chainbase::oid_ref<T>& var, variant& vo)
{
  vo = var.get_value();
}


template<typename T>
void from_variant(const variant& vo, chainbase::oid<T>& var)
{
  var = chainbase::oid<T>(vo.as_int64());
}

template<typename T>
void from_variant(const variant& vo, chainbase::oid_ref<T>& var)
{
  var = chainbase::oid<T>(vo.as_int64());
}

template< typename T >
struct get_typename< chainbase::oid< T > >
{
  static const char* name()
  {
    static std::string n = std::string("chainbase::oid<") + get_typename< T >::name() + ">";
    return n.c_str();
  }
};

template< typename T >
struct get_typename< chainbase::oid_ref< T > >
{
  static const char* name()
  {
    static std::string n = std::string("chainbase::oid_ref<") + get_typename< T >::name() + ">";
    return n.c_str();
  }
};

namespace raw
{

template<typename Stream, typename T>
inline void pack(Stream& s, const chainbase::oid<T>& id)
{
  s.write((const char*)&id, sizeof(id));
}

template<typename Stream, typename T>
inline void unpack(Stream& s, chainbase::oid<T>& id, uint32_t depth = 0, bool limit_is_disabled = false)
{
  s.read((char*)&id, sizeof(id));
}

template<typename Stream, typename T>
void pack(Stream& s, const chainbase::oid_ref<T>& id)
{
  s.write((const char*)&id, sizeof(id));
}

template<typename Stream, typename T>
void unpack(Stream& s, chainbase::oid_ref<T>& id, uint32_t depth = 0, bool limit_is_disabled = false)
{
  s.read((char*)&id, sizeof(id));
}

} /// namespace raw

} /// namespace fc
