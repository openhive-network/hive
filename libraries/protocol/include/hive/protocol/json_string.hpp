#pragma once
#include <fc/reflect/reflect.hpp>
#include <fc/io/raw.hpp>
#include <fc/reflect/variant.hpp>

namespace hive { namespace protocol {

  // the fc::json::fast_is_valid() method uses the simdjson parser, which requires all inputs to have some extra
  // padding at the end of the string.  The json_string class should act like a string and serialize like a string,
  // but it will always ensure that it always has the required extra padding so we don't have to reallocate it later
  // when validating
  class json_string
  {
    std::string s;
  public:
    constexpr static const size_t required_padding = 32; // this must match the SIMDJSON_PADDING constant
    explicit json_string(const std::string& other)
    {
      s.reserve(other.size() + required_padding);
      s = other;
      assert(s.capacity() - s.size() >= required_padding);
    }
    json_string()
    {
      s.reserve(required_padding);
      assert(s.capacity() - s.size() >= required_padding);
    }
    json_string(const json_string& other)
    {
      s.reserve(other.s.size() + required_padding);
      s = other.s;
      assert(s.capacity() - s.size() >= required_padding);
    }
    json_string(json_string&& other) : s(std::move(other.s))
    {
      assert(s.capacity() - s.size() >= required_padding);
    }
    json_string& operator=(const json_string& rhs)
    {
      s.reserve(rhs.s.size() + required_padding);
      s = rhs.s;
      assert(s.capacity() - s.size() >= required_padding);
      return *this;
    }
    json_string& operator=(const std::string& rhs)
    {
      s.reserve(rhs.size() + required_padding);
      s = rhs;
      assert(s.capacity() - s.size() >= required_padding);
      return *this;
    }
    template<typename Stream>
    void dump_to_stream(Stream& stream)const
    {
      fc::raw::pack(stream, s);
    }
    template<typename Stream>
    void load_from_stream(Stream& stream)
    {
      fc::raw::unpack(stream, s);
      s.reserve(s.size() + hive::protocol::json_string::required_padding);
    }
    operator const std::string&() const { return s; }
    // operator std::string&() { return s; } 
    bool operator==(const json_string& rhs) const { return s == rhs.s; }
    bool operator==(const char* rhs) const { return s == rhs; }
    bool operator<(const json_string& rhs) const { return s < rhs.s; }
    size_t size() const { return s.size(); }
    size_t length() const { return s.length(); }
    bool empty() const { return s.empty(); }
    void clear()
    { 
      s.clear();
      if (s.capacity() < required_padding)
        s.reserve(required_padding);
      assert(s.capacity() - s.size() >= required_padding);
    }
  };
} } // end hive::protocol

FC_REFLECT_TYPENAME( hive::protocol::json_string )

namespace fc
{
  inline void to_variant(const hive::protocol::json_string& json_string, variant& var)
  { try {
    to_variant(json_string.operator const std::string&(), var);
  } FC_CAPTURE_AND_RETHROW() }

  inline void from_variant(const variant& var, hive::protocol::json_string& json_string)
  { try {
    json_string = var.get_string();
  } FC_CAPTURE_AND_RETHROW() }

  template<typename Stream>
  inline Stream& operator<<(Stream& s, const hive::protocol::json_string& json_str)
  { try {
    json_str.dump_to_stream(s);

    return s;
  } FC_CAPTURE_AND_RETHROW() }
  template<typename Stream>
  inline Stream& operator>>(Stream& s, hive::protocol::json_string& json_str)
  { try {
    json_str.load_from_stream(s);

    return s;
  } FC_CAPTURE_AND_RETHROW() }
} // end namespace fc
