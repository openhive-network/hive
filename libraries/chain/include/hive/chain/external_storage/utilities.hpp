
#pragma once

#include <fc/io/raw.hpp>

#include<vector>

namespace hive { namespace chain {

typedef std::vector<char> serialize_buffer_t;

template <class T>
serialize_buffer_t dump(const T& obj)
{
  serialize_buffer_t serializedObj;
  auto size = fc::raw::pack_size(obj);
  serializedObj.resize(size);
  fc::datastream<char*> ds(serializedObj.data(), size);
  fc::raw::pack(ds, obj);
  return serializedObj;
}

template <class T>
void load(T& obj, const char* data, size_t size)
{
  fc::datastream<const char*> ds(data, size);
  fc::raw::unpack(ds, obj);
}

template <class T>
void load(T& obj, const serialize_buffer_t& source)
{
  load(obj, source.data(), source.size());
}

} } // hive::chain
