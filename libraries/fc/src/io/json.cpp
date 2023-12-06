#include <fc/io/json.hpp>
#include <fc/exception/exception.hpp>
#include <fc/io/iostream.hpp>
#include <fc/io/buffered_iostream.hpp>
#include <fc/io/fstream.hpp>
#include <fc/io/sstream.hpp>
#include <fc/log/logger.hpp>
// #include <utfcpp/utf8.h>
#include <iostream>
#include <fstream>
#include <sstream>

// #define SIMDJSON_DEVELOPMENT_CHECKS 1
#include <simdjson.h>

#include <boost/filesystem/fstream.hpp>

namespace fc
{
  // forward declarations of provided functions
  template <typename T, json::parse_type parser_type>
  variant variant_from_stream(T &in, const json::format_validation_mode json_validation_mode, uint32_t depth = 0);
  template <typename T>
  char parseEscape(T &in, uint32_t depth = 0);
  template <typename T>
  fc::string stringFromStream(T &in, uint32_t depth = 0);
  template <typename T>
  bool skip_white_space(T &in, uint32_t depth = 0);
  template <typename T>
  fc::string stringFromToken(T &in, uint32_t depth = 0);
  template <typename T, json::parse_type parser_type>
  variant_object objectFromStream(T &in, const json::format_validation_mode json_validation_mode, uint32_t depth = 0);
  template <typename T, json::parse_type parser_type>
  variants arrayFromStream(T &in, const json::format_validation_mode json_validation_mode, uint32_t depth = 0);
  template <typename T, json::parse_type parser_type>
  variant number_from_stream(T &in, uint32_t depth = 0);
  template <typename T>
  variant token_from_stream(T &in, uint32_t depth = 0);
  template <typename T>
  void escape_string(const string &str, T &os, uint32_t depth = 0);
  template <typename T>
  void to_stream(T &os, const variants &a, json::output_formatting format);
  template <typename T>
  void to_stream(T &os, const variant_object &o, json::output_formatting format);
  template <typename T>
  void to_stream(T &os, const variant &v, json::output_formatting format);
  fc::string pretty_print(const fc::string &v, uint8_t indent);
}

#include <fc/io/json_relaxed.hpp>

namespace fc
{
  class fast_stream
  {
  private:
    std::string content;

  public:
    fast_stream(uint32_t buffer_size = 10'000'000)
    {
      content.reserve(buffer_size);
    }

    fast_stream &operator<<(const char &v)
    {
      content += v;
      return *this;
    }

    fast_stream &operator<<(const char *v)
    {
      content.append(v, std::strlen(v));
      return *this;
    }

    fast_stream &operator<<(const std::string &v)
    {
      content.append(v);
      return *this;
    }

    template <typename T>
    fast_stream &operator<<(const T &v)
    {
      content.append(std::move(std::to_string(v)));
      return *this;
    }

    std::string &&str()
    {
      return std::move(content);
    }
  };

  template <typename T>
  char parseEscape(T &in, uint32_t)
  {
    if (in.peek() == '\\')
    {
      try
      {
        in.get();
        switch (in.peek())
        {
        case 't':
          in.get();
          return '\t';
        case 'n':
          in.get();
          return '\n';
        case 'r':
          in.get();
          return '\r';
        case '\\':
          in.get();
          return '\\';
        default:
          return in.get();
        }
      }
      FC_RETHROW_EXCEPTIONS(info, "Stream ended with '\\'");
    }
    FC_THROW_EXCEPTION(parse_error_exception, "Expected '\\'");
  }

  template <typename T>
  bool skip_white_space(T &in, uint32_t)
  {
    bool skipped = false;
    while (true)
    {
      switch (in.peek())
      {
      case ' ':
      case '\t':
      case '\n':
      case '\r':
        skipped = true;
        in.get();
        break;
      default:
        return skipped;
      }
    }
  }

  template <typename T>
  fc::string stringFromStream(T &in, uint32_t depth)
  {
    fc::string result;
    result.reserve(25);
    try
    {
      char c = in.peek();

      if (c != '"')
        FC_THROW_EXCEPTION(parse_error_exception,
                           "Expected '\"' but read '${char}'",
                           ("char", string(&c, (&c) + 1)));
      in.get();
      while (true)
      {

        switch (c = in.peek())
        {
        case '\\':
          result.push_back(parseEscape(in, depth));
          break;
        case 0x04:
          FC_THROW_EXCEPTION(parse_error_exception, "EOF before closing '\"' in string '${token}'",
                             ("token", result));
        case '"':
          in.get();
          return result;
        default:
          result.push_back(c);
          in.get();
        }
      }
      FC_THROW_EXCEPTION(parse_error_exception, "EOF before closing '\"' in string '${token}'",
                         ("token", result));
    }
    FC_RETHROW_EXCEPTIONS(warn, "while parsing token '${token}'",
                          ("token", result));
  }
  template <typename T>
  fc::string stringFromToken(T &in, uint32_t depth)
  {
    fc::stringstream token;
    try
    {
      char c = in.peek();

      while (true)
      {
        switch (c = in.peek())
        {
        case '\\':
          token << parseEscape(in, depth);
          break;
        case '\t':
        case ' ':
        case '\0':
        case '\n':
          in.get();
          return token.str();
        default:
          if (isalnum(c) || c == '_' || c == '-' || c == '.' || c == ':' || c == '/')
          {
            token << c;
            in.get();
          }
          else
            return token.str();
        }
      }
      return token.str();
    }
    catch (const fc::eof_exception &eof)
    {
      return token.str();
    }
    catch (const std::ios_base::failure &)
    {
      return token.str();
    }

    FC_RETHROW_EXCEPTIONS(warn, "while parsing token '${token}'",
                          ("token", token.str()));
  }

  template <typename T, json::parse_type parser_type>
  variant_object objectFromStream(T &in, const json::format_validation_mode json_validation_mode, uint32_t depth)
  {
    depth++;
    FC_ASSERT(depth <= JSON_MAX_RECURSION_DEPTH);
    mutable_variant_object obj;
    try
    {
      char c = in.peek();
      if (c != '{')
        FC_THROW_EXCEPTION(parse_error_exception,
                           "Expected '{', but read '${char}'",
                           ("char", string(&c, &c + 1)));
      in.get();
      skip_white_space(in, depth);
      while (in.peek() != '}')
      {
        if (in.peek() == ',')
        {
          in.get();
          continue;
        }
        if (skip_white_space(in, depth))
          continue;
        string key = stringFromStream(in, depth);
        skip_white_space(in, depth);
        if (in.peek() != ':')
        {
          FC_THROW_EXCEPTION(parse_error_exception, "Expected ':' after key \"${key}\"",
                             ("key", key));
        }
        in.get();
        auto val = variant_from_stream<T, parser_type>(in, json_validation_mode, depth);

        obj(std::move(key), std::move(val));
        skip_white_space(in, depth);
      }
      if (in.peek() == '}')
      {
        in.get();
        return obj;
      }
      FC_THROW_EXCEPTION(parse_error_exception, "Expected '}' after ${variant}", ("variant", obj));
    }
    catch (const fc::eof_exception &e)
    {
      FC_THROW_EXCEPTION(parse_error_exception, "Unexpected EOF: ${e}", ("e", e.to_detail_string()));
    }
    catch (const std::ios_base::failure &e)
    {
      FC_THROW_EXCEPTION(parse_error_exception, "Unexpected EOF: ${e}", ("e", e.what()));
    }
    FC_RETHROW_EXCEPTIONS(warn, "Error parsing object");
  }

  template <typename T, json::parse_type parser_type>
  variants arrayFromStream(T &in, const json::format_validation_mode json_validation_mode, uint32_t depth)
  {
    depth++;
    FC_ASSERT(depth <= JSON_MAX_RECURSION_DEPTH);
    variants ar;
    try
    {
      if (in.peek() != '[')
        FC_THROW_EXCEPTION(parse_error_exception, "Expected '['");

      in.get();
      skip_white_space(in, depth);
      bool expecting_new_element = true;

      while (in.peek() != ']')
      {
        if (in.peek() == ',')
        {
          if (json_validation_mode == json::format_validation_mode::full)
          {
            if (ar.empty())
              FC_THROW_EXCEPTION(parse_error_exception, "Invalid leading comma.");
            if (expecting_new_element)
              FC_THROW_EXCEPTION(parse_error_exception, "Found invalid comma instead of new array element.");
          }
          in.get();
          expecting_new_element = true;
          continue;
        }
        if (skip_white_space(in, depth))
          continue;
        if (json_validation_mode == json::format_validation_mode::full && !expecting_new_element)
          FC_THROW_EXCEPTION(parse_error_exception, "Missing ',' after object in json array.");

        ar.push_back(variant_from_stream<T, parser_type>(in, json_validation_mode, depth));
        expecting_new_element = false;
        skip_white_space(in, depth);
      }

      if (json_validation_mode == json::format_validation_mode::full && expecting_new_element && !ar.empty())
        FC_THROW_EXCEPTION(parse_error_exception, "Detected ',' after last array object. Expected ']'");

      in.get();
    }
    FC_RETHROW_EXCEPTIONS(warn, "Attempting to parse array ${array}",
                          ("array", ar));
    return ar;
  }

  template <typename T, json::parse_type parser_type>
  variant number_from_stream(T &in, uint32_t depth)
  {
    depth++;
    fc::string number_string;
    number_string.reserve(10);

    bool dot = false;
    bool neg = false;
    if (in.peek() == '-')
    {
      neg = true;
      number_string.push_back(in.get());
    }
    bool done = false;

    try
    {
      char c;
      while ((c = in.peek()) && !done)
      {

        switch (c)
        {
        case '.':
          if (dot)
            FC_THROW_EXCEPTION(parse_error_exception, "Can't parse a number with two decimal places");
          dot = true;
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
          number_string.push_back(in.get());
          break;
        default:
          if (isalnum(c))
          {
            return number_string + stringFromToken(in, depth);
          }
          done = true;
          break;
        }
      }
    }
    catch (fc::eof_exception &)
    {
    }
    catch (const std::ios_base::failure &)
    {
    }

    if (number_string == "-." || number_string == ".") // check the obviously wrong things we could have encountered
      FC_THROW_EXCEPTION(parse_error_exception, "Can't parse token \"${token}\" as a JSON numeric constant", ("token", number_string));
    if (dot)
      return parser_type == json::legacy_parser_with_string_doubles ? variant(number_string) : variant(to_double(number_string));
    if (neg)
      return to_int64(number_string);
    return to_uint64(number_string);
  }
  template <typename T>
  variant token_from_stream(T &in, uint32_t depth)
  {
    depth++;
    std::stringstream ss;
    ss.exceptions(std::ifstream::badbit);
    bool received_eof = false;
    bool done = false;

    try
    {
      char c;
      while ((c = in.peek()) && !done)
      {
        switch (c)
        {
        case 'n':
        case 'u':
        case 'l':
        case 't':
        case 'r':
        case 'e':
        case 'f':
        case 'a':
        case 's':
          ss.put(in.get());
          break;
        default:
          done = true;
          break;
        }
      }
    }
    catch (fc::eof_exception &)
    {
      received_eof = true;
    }
    catch (const std::ios_base::failure &)
    {
      received_eof = true;
    }

    // we can get here either by processing a delimiter as in "null,"
    // an EOF like "null<EOF>", or an invalid token like "nullZ"
    fc::string str = ss.str();
    if (str == "null")
      return variant();
    if (str == "true")
      return true;
    if (str == "false")
      return false;
    else
    {
      if (received_eof)
      {
        if (str.empty())
          FC_THROW_EXCEPTION(parse_error_exception, "Unexpected EOF");
        else
          return str;
      }
      else
      {
        // if we've reached this point, we've either seen a partial
        // token ("tru<EOF>") or something our simple parser couldn't
        // make out ("falfe")
        // A strict JSON parser would signal this as an error, but we
        // will just treat the malformed token as an un-quoted string.
        return str + stringFromToken(in, depth);
      }
    }
  }

  template <typename T, json::parse_type parser_type>
  variant variant_from_stream(T &in, const json::format_validation_mode json_validation_mode, uint32_t depth)
  {
    depth++;
    FC_ASSERT(depth <= JSON_MAX_RECURSION_DEPTH);
    skip_white_space(in, depth);
    variant var;
    while (true)
    {
      signed char c = in.peek();
      switch (c)
      {
      case ' ':
      case '\t':
      case '\n':
      case '\r':
        in.get();
        continue;
      case '"':
        return stringFromStream(in, depth);
      case '{':
        return objectFromStream<T, parser_type>(in, json_validation_mode, depth);
      case '[':
        return arrayFromStream<T, parser_type>(in, json_validation_mode, depth);
      case '-':
      case '.':
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
        return number_from_stream<T, parser_type>(in, depth);
      // null, true, false, or 'warning' / string
      case 'n':
      case 't':
      case 'f':
        return token_from_stream(in, depth);
      case 0x04: // ^D end of transmission
      case EOF:
      case 0:
        FC_THROW_EXCEPTION(eof_exception, "unexpected end of file");
      default:
        FC_THROW_EXCEPTION(parse_error_exception, "Unexpected char '${c}' in \"${s}\"",
                           ("c", c)("s", stringFromToken(in, depth)));
      }
    }
    return variant();
  }

  /** the purpose of this check is to verify that we will not get a stack overflow in the recursive descent parser */
  void check_string_depth(const string &utf8_str)
  {
    int32_t open_object = 0;
    int32_t open_array = 0;
    for (auto c : utf8_str)
    {
      switch (c)
      {
      case '{':
        open_object++;
        break;
      case '}':
        open_object--;
        break;
      case '[':
        open_array++;
        break;
      case ']':
        open_array--;
        break;
      default:
        break;
      }
      FC_ASSERT(open_object < 100 && open_array < 100, "object graph too deep", ("object depth", open_object)("array depth", open_array));
    }
  }

  variant json::from_string(const std::string &utf8_str, const format_validation_mode json_validation_mode, parse_type ptype, uint32_t depth)
  {
    try
    {
      depth++;
      FC_ASSERT(depth <= JSON_MAX_RECURSION_DEPTH);
      check_string_depth(utf8_str);

      fc::stringstream in(utf8_str);
      // in.exceptions( std::ifstream::eofbit );
      switch (ptype)
      {
      case legacy_parser:
        return variant_from_stream<fc::stringstream, legacy_parser>(in, json_validation_mode, depth);
      case legacy_parser_with_string_doubles:
        return variant_from_stream<fc::stringstream, legacy_parser_with_string_doubles>(in, json_validation_mode, depth);
      case strict_parser:
        return json_relaxed::variant_from_stream<fc::stringstream, true>(in, depth);
      case relaxed_parser:
        return json_relaxed::variant_from_stream<fc::stringstream, false>(in, depth);
      default:
        FC_ASSERT(false, "Unknown JSON parser type {ptype}", ("ptype", ptype));
      }
    }
    FC_RETHROW_EXCEPTIONS(warn, "", ("str", utf8_str))
  }

  variants json::variants_from_string(const std::string &utf8_str, parse_type ptype, uint32_t depth)
  {
    try
    {
      depth++;
      FC_ASSERT(depth <= JSON_MAX_RECURSION_DEPTH);
      check_string_depth(utf8_str);
      variants result;
      fc::stringstream in(utf8_str);
      // in.exceptions( std::ifstream::eofbit );
      try
      {
        while (true)
        {
          // result.push_back( variant_from_stream( in ));
          result.push_back(json_relaxed::variant_from_stream<fc::stringstream, false>(in, depth));
        }
      }
      catch (const fc::eof_exception &)
      {
      }
      return result;
    }
    FC_RETHROW_EXCEPTIONS(warn, "", ("str", utf8_str))
  }
  /*
  void toUTF8( const char str, ostream& os )
  {
     // validate str == valid utf8
     utf8::replace_invalid( &str, &str + 1, ostream_iterator<char>(os) );
  }

  void toUTF8( const wchar_t c, ostream& os )
  {
     utf8::utf16to8( &c, (&c)+1, ostream_iterator<char>(os) );
  }
  */

  /**
   *  Convert '\t', '\a', '\n', '\\' and '"'  to "\t\a\n\\\""
   *
   *  All other characters are printed as UTF8.
   */
  template <typename T>
  void escape_string(const string &str, T &os, uint32_t)
  {
    os << '"';
    for (auto itr = str.begin(); itr != str.end(); ++itr)
    {
      switch (*itr)
      {
      case '\b': // \x08
        os << "\\b";
        break;
      case '\f': // \x0c
        os << "\\f";
        break;
      case '\n': // \x0a
        os << "\\n";
        break;
      case '\r': // \x0d
        os << "\\r";
        break;
      case '\t': // \x09
        os << "\\t";
        break;
      case '\\':
        os << "\\\\";
        break;
      case '\"':
        os << "\\\"";
        break;
      case '\x00':
        os << "\\u0000";
        break;
      case '\x01':
        os << "\\u0001";
        break;
      case '\x02':
        os << "\\u0002";
        break;
      case '\x03':
        os << "\\u0003";
        break;
      case '\x04':
        os << "\\u0004";
        break;
      case '\x05':
        os << "\\u0005";
        break;
      case '\x06':
        os << "\\u0006";
        break;
      case '\x07':
        os << "\\u0007";
        break; // \a is not valid JSON
               // case '\x08': os << "\\u0008"; break; // \b
               // case '\x09': os << "\\u0009"; break; // \t
               // case '\x0a': os << "\\u000a"; break; // \n
      case '\x0b':
        os << "\\u000b";
        break;
        // case '\x0c': os << "\\u000c"; break; // \f
        // case '\x0d': os << "\\u000d"; break; // \r
      case '\x0e':
        os << "\\u000e";
        break;
      case '\x0f':
        os << "\\u000f";
        break;

      case '\x10':
        os << "\\u0010";
        break;
      case '\x11':
        os << "\\u0011";
        break;
      case '\x12':
        os << "\\u0012";
        break;
      case '\x13':
        os << "\\u0013";
        break;
      case '\x14':
        os << "\\u0014";
        break;
      case '\x15':
        os << "\\u0015";
        break;
      case '\x16':
        os << "\\u0016";
        break;
      case '\x17':
        os << "\\u0017";
        break;
      case '\x18':
        os << "\\u0018";
        break;
      case '\x19':
        os << "\\u0019";
        break;
      case '\x1a':
        os << "\\u001a";
        break;
      case '\x1b':
        os << "\\u001b";
        break;
      case '\x1c':
        os << "\\u001c";
        break;
      case '\x1d':
        os << "\\u001d";
        break;
      case '\x1e':
        os << "\\u001e";
        break;
      case '\x1f':
        os << "\\u001f";
        break;

      default:
        os << *itr;
        // toUTF8( *itr, os );
      }
    }
    os << '"';
  }
  ostream &json::to_stream(ostream &out, const fc::string &str)
  {
    escape_string(str, out);
    return out;
  }

  template <typename T>
  void to_stream(T &os, const variants &a, json::output_formatting format)
  {
    os << '[';
    auto itr = a.begin();

    while (itr != a.end())
    {
      to_stream(os, *itr, format);
      ++itr;
      if (itr != a.end())
        os << ',';
    }
    os << ']';
  }
  template <typename T>
  void to_stream(T &os, const variant_object &o, json::output_formatting format)
  {
    os << '{';
    auto itr = o.begin();

    while (itr != o.end())
    {
      escape_string(itr->key(), os);
      os << ':';
      to_stream(os, itr->value(), format);
      ++itr;
      if (itr != o.end())
        os << ',';
    }
    os << '}';
  }

  template <typename T>
  void to_stream(T &os, const variant &v, json::output_formatting format)
  {
    switch (v.get_type())
    {
    case variant::null_type:
      os << "null";
      return;
    case variant::int64_type:
    {
      int64_t i = v.as_int64();
      if (format == json::stringify_large_ints_and_doubles &&
          (i > json::json_integer_limits::max_positive_value ||
           i < json::json_integer_limits::max_negative_value))
        os << '"' << v.as_string() << '"';
      else
        os << i;

      return;
    }
    case variant::uint64_type:
    {
      uint64_t i = v.as_uint64();
      if (format == json::stringify_large_ints_and_doubles &&
          i > static_cast<uint64_t>(json::json_integer_limits::max_positive_value))
        os << '"' << v.as_string() << '"';
      else
        os << i;

      return;
    }
    case variant::double_type:
      if (format == json::stringify_large_ints_and_doubles)
        os << '"' << v.as_string() << '"';
      else
        os << v.as_string();
      return;
    case variant::bool_type:
      os << v.as_string();
      return;
    case variant::string_type:
      escape_string(v.get_string(), os);
      return;
    case variant::blob_type:
      escape_string(v.as_string(), os);
      return;
    case variant::array_type:
    {
      const variants &a = v.get_array();
      to_stream(os, a, format);
      return;
    }
    case variant::object_type:
    {
      const variant_object &o = v.get_object();
      to_stream(os, o, format);
      return;
    }
    }
  }

  fc::string json::to_string(const variant &v, output_formatting format /* = stringify_large_ints_and_doubles */)
  {
    fc::fast_stream ss;
    fc::to_stream(ss, v, format);
    return ss.str();
  }

  fc::string pretty_print(const fc::string &v, uint8_t indent)
  {
    int level = 0;
    fc::stringstream ss;
    bool first = false;
    bool quote = false;
    bool escape = false;
    for (uint32_t i = 0; i < v.size(); ++i)
    {
      switch (v[i])
      {
      case '\\':
        if (!escape)
        {
          if (quote)
            escape = true;
        }
        else
        {
          escape = false;
        }
        ss << v[i];
        break;
      case ':':
        if (!quote)
        {
          ss << ": ";
        }
        else
        {
          ss << ':';
        }
        break;
      case '"':
        if (first)
        {
          ss << '\n';
          for (int i = 0; i < level * indent; ++i)
            ss << ' ';
          first = false;
        }
        if (!escape)
        {
          quote = !quote;
        }
        escape = false;
        ss << '"';
        break;
      case '{':
      case '[':
        ss << v[i];
        if (!quote)
        {
          ++level;
          first = true;
        }
        else
        {
          escape = false;
        }
        break;
      case '}':
      case ']':
        if (!quote)
        {
          if (v[i - 1] != '[' && v[i - 1] != '{')
          {
            ss << '\n';
          }
          --level;
          if (!first)
          {
            for (int i = 0; i < level * indent; ++i)
              ss << ' ';
          }
          first = false;
          ss << v[i];
          break;
        }
        else
        {
          escape = false;
          ss << v[i];
        }
        break;
      case ',':
        if (!quote)
        {
          ss << ',';
          first = true;
        }
        else
        {
          escape = false;
          ss << ',';
        }
        break;
      case 'n':
        // If we're in quotes and see a \n, just print it literally but unset the escape flag.
        if (quote && escape)
          escape = false;
        // No break; fall through to default case
      default:
        if (first)
        {
          ss << '\n';
          for (int i = 0; i < level * indent; ++i)
            ss << ' ';
          first = false;
        }
        ss << v[i];
      }
    }
    return ss.str();
  }

  fc::string json::to_pretty_string(const variant &v, output_formatting format /* = stringify_large_ints_and_doubles */)
  {
    return pretty_print(to_string(v, format), 2);
  }

  void json::save_to_file(const variant &v, const fc::path &fi, bool pretty, output_formatting format /* = stringify_large_ints_and_doubles */)
  {
    if (pretty)
    {
      auto str = json::to_pretty_string(v, format);
      fc::ofstream o(fi);
      o.write(str.c_str(), str.size());
    }
    else
    {
      fc::ofstream o(fi);
      fc::to_stream(o, v, format);
    }
  }
  variant json::from_file(const fc::path &p, parse_type ptype, uint32_t depth)
  {
    // auto tmp = std::make_shared<fc::ifstream>( p, ifstream::binary );
    // auto tmp = std::make_shared<std::ifstream>( p.generic_string().c_str(), std::ios::binary );
    // buffered_istream bi( tmp );
    boost::filesystem::ifstream bi(p, std::ios::binary);
    switch (ptype)
    {
    case legacy_parser:
      return variant_from_stream<boost::filesystem::ifstream, legacy_parser>(bi, format_validation_mode::full /* format_validation_mode*/, depth);
    case legacy_parser_with_string_doubles:
      return variant_from_stream<boost::filesystem::ifstream, legacy_parser_with_string_doubles>(bi, format_validation_mode::full /* format_validation_mode*/, depth);
    case strict_parser:
      return json_relaxed::variant_from_stream<boost::filesystem::ifstream, true>(bi, depth);
    case relaxed_parser:
      return json_relaxed::variant_from_stream<boost::filesystem::ifstream, false>(bi, depth);
    default:
      FC_ASSERT(false, "Unknown JSON parser type {ptype}", ("ptype", ptype));
    }
  }
  variant json::from_stream(buffered_istream &in, parse_type ptype, uint32_t depth)
  {
    depth++;
    FC_ASSERT(depth <= JSON_MAX_RECURSION_DEPTH);
    switch (ptype)
    {
    case legacy_parser:
      return variant_from_stream<fc::buffered_istream, legacy_parser>(in, format_validation_mode::full /* json_validation_mode*/, depth);
    case legacy_parser_with_string_doubles:
      return variant_from_stream<fc::buffered_istream, legacy_parser_with_string_doubles>(in, format_validation_mode::full /* json_validation_mode*/, depth);
    case strict_parser:
      return json_relaxed::variant_from_stream<buffered_istream, true>(in, depth);
    case relaxed_parser:
      return json_relaxed::variant_from_stream<buffered_istream, false>(in, depth);
    default:
      FC_ASSERT(false, "Unknown JSON parser type {ptype}", ("ptype", ptype));
    }
  }

  ostream &json::to_stream(ostream &out, const variant &v, output_formatting format /* = stringify_large_ints_and_doubles */)
  {
    fc::to_stream(out, v, format);
    return out;
  }
  ostream &json::to_stream(ostream &out, const variants &v, output_formatting format /* = stringify_large_ints_and_doubles */)
  {
    fc::to_stream(out, v, format);
    return out;
  }
  ostream &json::to_stream(ostream &out, const variant_object &v, output_formatting format /* = stringify_large_ints_and_doubles */)
  {
    fc::to_stream(out, v, format);
    return out;
  }

  bool json::is_valid(const std::string &utf8_str, const format_validation_mode json_validation_mode, parse_type ptype, uint32_t depth)
  {
    if (utf8_str.size() == 0)
      return false;
    fc::stringstream in(utf8_str);
    switch (ptype)
    {
    case legacy_parser:
      variant_from_stream<fc::stringstream, legacy_parser>(in, json_validation_mode, depth);
      break;
    case legacy_parser_with_string_doubles:
      variant_from_stream<fc::stringstream, legacy_parser_with_string_doubles>(in, json_validation_mode, depth);
      break;
    case strict_parser:
      json_relaxed::variant_from_stream<fc::stringstream, true>(in, depth);
      break;
    case relaxed_parser:
      json_relaxed::variant_from_stream<fc::stringstream, false>(in, depth);
      break;
    default:
      FC_ASSERT(false, "Unknown JSON parser type {ptype}", ("ptype", ptype));
    }
    try
    {
      in.peek();
    }
    catch (const eof_exception &e)
    {
      return true;
    }
    return false;
  }

  namespace
  {
    variant parse_element(simdjson::ondemand::value element)
    {
      switch (element.type())
      {
      case simdjson::ondemand::json_type::array:
      {
        variants arr;
        arr.reserve(element.count_elements());
        auto array = element.get_array();
        std::transform(array.begin(), array.end(), std::back_inserter(arr), parse_element);
        return arr;
      }
      case simdjson::ondemand::json_type::object:
      {
        mutable_variant_object obj;
        auto object = element.get_object();
        std::for_each(object.begin(), object.end(), [&obj](simdjson::ondemand::field field)
                      {
                     variant value = parse_element(field.value());
                     obj(std::string((std::string_view)field.unescaped_key()), std::move(value)); });
        return obj;
      }
      case simdjson::ondemand::json_type::number:
        switch (element.get_number_type())
        {
        case simdjson::ondemand::number_type::signed_integer:
          return variant((int64_t)element.get_int64());
        case simdjson::ondemand::number_type::unsigned_integer:
          return variant((uint64_t)element.get_uint64());
        case simdjson::ondemand::number_type::floating_point_number:
        default:
          return variant((double)element.get_double());
        }
      case simdjson::ondemand::json_type::string:
        return variant(std::string((std::string_view)element.get_string()));
      case simdjson::ondemand::json_type::boolean:
        return variant((bool)element.get_bool());
      case simdjson::ondemand::json_type::null:
        return variant(nullptr);
      default:
        FC_THROW("Encountered an unknown type during json parsing");
      }
    }
    variant parse_document(simdjson::ondemand::document &doc)
    {
      switch (doc.type())
      {
      case simdjson::ondemand::json_type::number:
        switch (doc.get_number_type())
        {
        case simdjson::ondemand::number_type::signed_integer:
          return variant((int64_t)doc.get_int64());
        case simdjson::ondemand::number_type::unsigned_integer:
          return variant((uint64_t)doc.get_uint64());
        case simdjson::ondemand::number_type::floating_point_number:
        default:
          return variant((double)doc.get_double());
        }
      case simdjson::ondemand::json_type::string:
      {
        std::string_view string_value = doc.get_string();
        return variant(std::string(string_value));
      }
      case simdjson::ondemand::json_type::boolean:
      {
        bool bool_value = doc.get_bool();
        return variant(bool_value);
      }
      case simdjson::ondemand::json_type::null:
        return variant(nullptr);
      default:
        return parse_element(doc);
      }
    }
  } // end anonymous namespace

  variant json::fast_from_string(const std::string &string_to_parse)
  {
    try
    {
      assert(string_to_parse.capacity() - string_to_parse.size() >= simdjson::SIMDJSON_PADDING);
      FC_ASSERT(string_to_parse.capacity() - string_to_parse.size() >= simdjson::SIMDJSON_PADDING,
                "this function requires the input to have ${bytes} bytes of extra padding",
                ("bytes", simdjson::SIMDJSON_PADDING));
      thread_local simdjson::ondemand::parser parser;
      simdjson::ondemand::document doc = parser.iterate(string_to_parse);
      return parse_document(doc);
    }
    FC_CAPTURE_AND_RETHROW((string_to_parse))
  }

  namespace
  {
    void validate_element(simdjson::ondemand::value element)
    {
      switch (element.type())
      {
      case simdjson::ondemand::json_type::array:
      {
        auto array = element.get_array();
        std::for_each(array.begin(), array.end(), validate_element);
        break;
      }
      case simdjson::ondemand::json_type::object:
      {
        auto object = element.get_object();
        std::for_each(object.begin(), object.end(), [](simdjson::ondemand::field field)
                      {
                     (void)field.unescaped_key().value();
                     validate_element(field.value()); });
        break;
      }
      case simdjson::ondemand::json_type::number:
        switch (element.get_number_type())
        {
        case simdjson::ondemand::number_type::signed_integer:
          (void)element.get_int64().value();
          break;
        case simdjson::ondemand::number_type::unsigned_integer:
          (void)element.get_uint64().value();
          break;
        case simdjson::ondemand::number_type::floating_point_number:
        default:
          (void)element.get_double().value();
        }
        break;
      case simdjson::ondemand::json_type::string:
        (void)element.get_string().value();
        break;
      case simdjson::ondemand::json_type::boolean:
        (void)element.get_bool().value();
        break;
      case simdjson::ondemand::json_type::null:
        (void)element.get_string().value();
        break;
      default:
        FC_THROW("Encountered an unknown type during json parsing");
      }
    }
    void validate_document(simdjson::ondemand::document &doc)
    {
      switch (doc.type())
      {
      case simdjson::ondemand::json_type::number:
        switch (doc.get_number_type())
        {
        case simdjson::ondemand::number_type::signed_integer:
          (void)doc.get_int64().value();
          break;
        case simdjson::ondemand::number_type::unsigned_integer:
          (void)doc.get_uint64().value();
          break;
        case simdjson::ondemand::number_type::floating_point_number:
        default:
          (void)doc.get_double().value();
        }
        break;
      case simdjson::ondemand::json_type::string:
        (void)doc.get_string().value();
        break;
      case simdjson::ondemand::json_type::boolean:
        (void)doc.get_bool().value();
        break;
      case simdjson::ondemand::json_type::null:
        (void)doc.get_string().value();
        break;
      default:
        return validate_element(doc);
      }
    }
  } // end anonymous namespace
  bool json::fast_is_valid(const std::string &string_to_validate)
  {
    assert(string_to_validate.capacity() - string_to_validate.size() >= simdjson::SIMDJSON_PADDING);
    FC_ASSERT(string_to_validate.capacity() - string_to_validate.size() >= simdjson::SIMDJSON_PADDING,
              "this function requires the input to have ${bytes} bytes of extra padding",
              ("bytes", simdjson::SIMDJSON_PADDING));
    thread_local simdjson::ondemand::parser parser;
    try
    {
      simdjson::ondemand::document doc = parser.iterate(string_to_validate);
      validate_document(doc);
      return true;
    }
    catch (...)
    {
      return false;
    }
  }
} // fc
