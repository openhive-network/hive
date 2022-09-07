#pragma once
#include <fc/reflect/reflect.hpp>
#include <fc/io/raw.hpp>
#include <fc/reflect/variant.hpp>

namespace hive { namespace protocol {
  class json_string;
} }
namespace fc
{
  inline void to_variant(const hive::protocol::json_string& json_string, variant& var);
  inline void from_variant(const variant& var, hive::protocol::json_string& json_string);
  namespace raw
  {
    template<typename Stream>
    inline Stream& operator<<(Stream& s, const hive::protocol::json_string& json_string);
    template<typename Stream>
    inline Stream& operator>>(Stream& s, hive::protocol::json_string& json_string);
  }

}

namespace hive { namespace protocol {

  // the fc::json::fast_is_valid() method uses the simdjson parser, which requires all inputs to have some extra
  // padding at the end of the string.  The json_string class should act like a string and serialize like a string,
  // but it will always ensure that it always has the required extra padding so we don't have to reallocate it later
  // when validating
  class json_string
  {
    friend inline void fc::to_variant(const json_string& json_string, fc::variant& var);
    friend inline void fc::from_variant(const fc::variant& var, json_string& json_string);
    template<typename Stream> friend inline Stream& fc::raw::operator<<(Stream& s, const hive::protocol::json_string& json_string);
    template<typename Stream> friend inline Stream& fc::raw::operator>>(Stream& s, hive::protocol::json_string& json_string);

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
    to_variant(json_string.s, var);
  } FC_CAPTURE_AND_RETHROW() }

  inline void from_variant(const variant& var, hive::protocol::json_string& json_string)
  { try {
    const std::string& source = var.get_string();
    json_string.s.reserve(source.size() + hive::protocol::json_string::required_padding);
    json_string.s = source;
    assert(json_string.s.capacity() - json_string.s.size() >= hive::protocol::json_string::required_padding);
  } FC_CAPTURE_AND_RETHROW() }

  namespace raw
  {
    template<typename Stream>
    inline Stream& operator<<(Stream& s, const hive::protocol::json_string& json_str)
    { try {
      pack(s, json_str.s);
      return s;
    } FC_CAPTURE_AND_RETHROW() }
    template<typename Stream>
    inline Stream& operator>>(Stream& s, hive::protocol::json_string& json_str)
    { try {
      unsigned_int string_length;
      unpack(s, string_length);
      json_str.s.reserve(string_length.value + hive::protocol::json_string::required_padding);
      json_str.s.resize(string_length.value);
      if (string_length.value)
        s.read(&json_str.s[0], string_length.value);
      return s;
    } FC_CAPTURE_AND_RETHROW() }
  }
} // end namespace fc
