#pragma once
#include <fc/variant.hpp>
#include <fc/filesystem.hpp>

#define JSON_MAX_RECURSION_DEPTH (200)

namespace fc
{
  class ostream;
  class buffered_istream;

  /**
   *  Provides interface for json serialization.
   *
   *  json strings are always UTF8
   */
  class json
  {
  public:
    enum parse_type
    {
      legacy_parser = 0,
      strict_parser = 1,
      relaxed_parser = 2,
      legacy_parser_with_string_doubles = 3
    };
    enum output_formatting
    {
      stringify_large_ints_and_doubles = 0,
      legacy_generator = 1
    };
    // Javascript needs to use bigint type for values which exceeses below limits
    enum json_integer_limits : int64_t
    {
      max_negative_value = -0x1fffffffffffff,
      max_positive_value = 0x1fffffffffffff
    };

    enum format_validation_mode : uint8_t
    {
      full = 0,
      relaxed = 1 // allows json formats which could be passed in previous versions of hived
    };

    static ostream &to_stream(ostream &out, const fc::string &);
    static ostream &to_stream(ostream &out, const variant &v, output_formatting format = stringify_large_ints_and_doubles);
    static ostream &to_stream(ostream &out, const variants &v, output_formatting format = stringify_large_ints_and_doubles);
    static ostream &to_stream(ostream &out, const variant_object &v, output_formatting format = stringify_large_ints_and_doubles);

    static variant from_stream(buffered_istream &in, const format_validation_mode json_validation_mode, parse_type ptype = legacy_parser, uint32_t depth = 0);

    static variant from_string(const string &utf8_str, const format_validation_mode json_validation_mode, parse_type ptype = legacy_parser, uint32_t depth = 0);
    static variant fast_from_string(const string &utf8_str);
    static variants variants_from_string(const string &utf8_str, parse_type ptype = legacy_parser, uint32_t depth = 0);
    static string to_string(const variant &v, output_formatting format = stringify_large_ints_and_doubles);
    static string to_pretty_string(const variant &v, output_formatting format = stringify_large_ints_and_doubles);

    static bool is_valid(const std::string &json_str, const format_validation_mode json_validation_mode, parse_type ptype = legacy_parser, uint32_t depth = 0);
    static bool fast_is_valid(const std::string &json_str);

    template <typename T>
    static void save_to_file(const T &v, const fc::path &fi, bool pretty = true, output_formatting format = stringify_large_ints_and_doubles)
    {
      save_to_file(variant(v), fi, pretty, format);
    }

    static void save_to_file(const variant &v, const fc::path &fi, bool pretty = true, output_formatting format = stringify_large_ints_and_doubles);
    static variant from_file(const fc::path &p, const format_validation_mode json_validation_mode, parse_type ptype = legacy_parser, uint32_t depth = 0);

    template <typename T>
    static T from_file(const fc::path &p, const format_validation_mode json_validation_mode, parse_type ptype = legacy_parser, uint32_t depth = 0)
    {
      depth++;
      return json::from_file(p, json_validation_mode, ptype, depth).as<T>();
    }

    template <typename T>
    static string to_string(const T &v, output_formatting format = stringify_large_ints_and_doubles)
    {
      return to_string(variant(v), format);
    }

    template <typename T>
    static string to_pretty_string(const T &v, output_formatting format = stringify_large_ints_and_doubles)
    {
      return to_pretty_string(variant(v), format);
    }

    template <typename T>
    static void save_to_file(const T &v, const std::string &p, bool pretty = true, output_formatting format = stringify_large_ints_and_doubles)
    {
      save_to_file(variant(v), fc::path(p), pretty);
    }
  };

} // fc
