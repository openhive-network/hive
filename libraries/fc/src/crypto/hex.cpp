#include <fc/crypto/hex.hpp>
#include <fc/exception/exception.hpp>

namespace fc {

namespace __detail {
    struct __hex_lookup_table {
    private:
      uint8_t _m_lookup_table[128];

    public:
      constexpr __hex_lookup_table() : _m_lookup_table()
      {
        for(uint8_t* mlt = _m_lookup_table; mlt < _m_lookup_table + 128; ++mlt)
          *mlt = 16;

        _m_lookup_table['0'] = 0;
        _m_lookup_table['1'] = 1;
        _m_lookup_table['2'] = 2;
        _m_lookup_table['3'] = 3;
        _m_lookup_table['4'] = 4;
        _m_lookup_table['5'] = 5;
        _m_lookup_table['6'] = 6;
        _m_lookup_table['7'] = 7;
        _m_lookup_table['8'] = 8;
        _m_lookup_table['9'] = 9;
        _m_lookup_table['a'] = 10;
        _m_lookup_table['b'] = 11;
        _m_lookup_table['c'] = 12;
        _m_lookup_table['d'] = 13;
        _m_lookup_table['e'] = 14;
        _m_lookup_table['f'] = 15;
        _m_lookup_table['A'] = 10;
        _m_lookup_table['B'] = 11;
        _m_lookup_table['C'] = 12;
        _m_lookup_table['D'] = 13;
        _m_lookup_table['E'] = 14;
        _m_lookup_table['F'] = 15;
      }
      constexpr uint8_t operator[](char const idx) const
      {
        if( UNLIKELY(_m_lookup_table[(std::size_t) idx] > 15) )
            FC_THROW_EXCEPTION( exception, "Invalid hex character '${c}'", ("c", fc::string(&idx,1) ) );

        return _m_lookup_table[(std::size_t) idx];
      }
    } constexpr hex_lookup_table;
}

    uint8_t from_hex( char c ) {
        return __detail::hex_lookup_table[c];
    }

    std::string to_hex( const char* d, uint32_t s ) 
    {
        std::string r;
        r.resize(s*2);
        const char* to_hex="0123456789abcdef";
        uint8_t* c = (uint8_t*)d;
        for( uint32_t i = 0; i < s; ++i )
        {
          r[i*2] = to_hex[(c[i]>>4)];
          r[i*2+1] = to_hex[(c[i] &0x0f)];
        }
        return r;
    }

    size_t from_hex( const fc::string& hex_str, char* out_data, size_t out_data_len ) {
        fc::string::const_iterator i = hex_str.begin();
        uint8_t* out_pos = (uint8_t*)out_data;
        uint8_t* out_end = out_pos + out_data_len;
        while( i != hex_str.end() && out_end != out_pos ) {
          *out_pos = from_hex( *i ) << 4;   
          ++i;
          if( i != hex_str.end() )  {
              *out_pos |= from_hex( *i );
              ++i;
          }
          ++out_pos;
        }
        return out_pos - (uint8_t*)out_data;
    }
    std::string to_hex( const std::vector<char>& data )
    {
       if( data.size() )
          return to_hex( data.data(), data.size() );
       return "";
    }

}
