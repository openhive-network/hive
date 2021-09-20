#include <hive/plugins/sql_serializer/data_2_sql_tuple_base.h>


namespace hive{ namespace plugins{ namespace sql_serializer {
  std::string
  data2_sql_tuple_base::escape(const std::string& source) const
  {
    return escape_sql(source);
  }

  std::string
  data2_sql_tuple_base::escape_raw(const fc::ripemd160& hash) const
  {
    return '\'' + fc::to_hex(hash.data(), hash.data_size()) + '\'';
  }

  std::string
  data2_sql_tuple_base::escape_raw(const fc::optional<signature_type>& sign) const
  {
    if( sign.valid() )
      return '\'' + fc::to_hex(reinterpret_cast<const char*>( sign->begin() ), sign->size()) + '\'';
    else
      return "NULL";
  }

  fc::string
  data2_sql_tuple_base::escape_sql(const std::string &text) const
  {
    if(text.empty()) return "E''";

    std::wstring utf32;
    utf32.reserve( text.size() );
    fc::decodeUtf8( text, &utf32 );

    std::string ret;
    ret.reserve( 6 * text.size() );

    ret = "E'";

    for (auto it = utf32.begin(); it != utf32.end(); it++)
    {

      const wchar_t& c{*it};
      const int code = static_cast<int>(c);

      if( code == 0 ) ret += " ";
      if(code > 0 && code <= 0x7F && std::isprint(code)) // if printable ASCII
        {
        switch(c)
        {
          case L'\r': ret += "\\015"; break;
          case L'\n': ret += "\\012"; break;
          case L'\v': ret += "\\013"; break;
          case L'\f': ret += "\\014"; break;
          case L'\\': ret += "\\134"; break;
          case L'\'': ret += "\\047"; break;
          case L'%':  ret += "\\045"; break;
          case L'_':  ret += "\\137"; break;
          case L':':  ret += "\\072"; break;
          default:    ret += static_cast<char>(code); break;
        }
        }
      else
      {
        fc::string u_code{};
        u_code.reserve(8);

        const int i_c = int(c);
        const char * c_str = reinterpret_cast<const char*>(&i_c);
        for( int _s = ( i_c > 0xff ) + ( i_c > 0xffff ) + ( i_c > 0xffffff ); _s >= 0; _s-- )
          u_code += fc::to_hex( c_str + _s, 1 );

        if(i_c > 0xffff)
        {
          ret += "\\U";
          if(u_code.size() < 8) ret.insert( ret.end(), 8 - u_code.size(), '0' );
        }
        else
        {
          ret += "\\u";
          if(u_code.size() < 4) ret.insert( ret.end(), 4 - u_code.size(), '0' );
        }
        ret += u_code;
      }
    }

    ret += '\'';
    return ret;
  }
}}} // namespace hive::plugins::sql_serializer

