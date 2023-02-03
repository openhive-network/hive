#pragma once
#include <fc/io/raw_fwd.hpp>


namespace fc { namespace fixed_string_wrapper {

  class verifier_switch
  {
    private:
      thread_local static bool verify;

    public:
      static void set_verify( bool val ){ verify = val; }
      static bool is_verifying_enabled() { return verify; };
  };

   /**
    *  This class is designed to offer in-place memory allocation of a string up to Length equal to
    *  sizeof(Storage).
    *
    *  The string will serialize the same way as std::string for variant and raw formats
    *  The string will sort according to the comparison operators defined for Storage, this enables effecient
    *  sorting.
    */
   template<typename Storage = std::pair<uint64_t,uint64_t> >
   class fixed_string {
      public:
         fixed_string(){}
         fixed_string( const fixed_string& c ):data(c.data){}

         fixed_string( const std::string& str ) {
           assign(str);
         }
         fixed_string( const char* str ) {
            assign(str, strlen(str));
         }

         operator std::string()const {
            const char* self = (const char*)&data;
            return std::string( self, self + size() );
         }

         uint32_t size()const {
            if( *(((const char*)&data)+sizeof(data) - 1) )
               return sizeof(data);
            return strnlen( (const char*)&data, sizeof(data) );
         }
         uint32_t length()const { return size(); }

         fixed_string& operator=( const fixed_string& str ) {
            data = str.data;
            return *this;
         }
         fixed_string& operator=( const char* str ) {
            return *this = fixed_string(str);
         }

         fixed_string& operator=( const std::string& str ) {
            if( str.size() <= sizeof(data) )
               data = Storage();

            assign(str);

            return *this;
         }

         friend std::string operator + ( const fixed_string& a, const std::string& b ) {
            return std::string(a) + b;
         }
         friend std::string operator + ( const std::string& a, const fixed_string& b ) {
            return a + std::string(b);
         }

         friend bool operator < ( const fixed_string& a, const fixed_string& b ) {
            return a.data < b.data;
         }
         friend bool operator <= ( const fixed_string& a, const fixed_string& b ) {
            return a.data <= b.data;
         }
         friend bool operator > ( const fixed_string& a, const fixed_string& b ) {
            return a.data > b.data;
         }
         friend bool operator >= ( const fixed_string& a, const fixed_string& b ) {
            return a.data >= b.data;
         }
         friend bool operator == ( const fixed_string& a, const fixed_string& b ) {
            return a.data == b.data;
         }
         friend bool operator != ( const fixed_string& a, const fixed_string& b ) {
            return a.data != b.data;
         }

      private:
        void assign(const char* in, size_t in_len) const
        {
            if( verifier_switch::is_verifying_enabled() )
            {
               FC_ASSERT(in_len <= sizeof(data), "Input too large: `${in}` (${is}) for fixed size string: (${fs})", (in)("is", in_len)("fs", sizeof(data)));
               memcpy( (char*)&data, in, in_len );
            }
            else
            {
               if( in_len <= sizeof(data) )
                  memcpy( (char*)&data, in, in_len );
               else
                  memcpy( (char*)&data, in, sizeof(data) );
            }
        }

        void assign(const std::string& in) const
        {
          assign(in.c_str(), in.size());
        }

      //private:
         Storage data;
   };

  namespace raw
  {
    template<typename Stream, typename Storage>
    inline void pack( Stream& s, const fc::fixed_string_wrapper::fixed_string<Storage>& u ) {
       unsigned_int size = u.size();
       pack( s, size );
       s.write( (const char*)&u.data, size );
    }

    template<typename Stream, typename Storage>
    inline void unpack( Stream& s, fc::fixed_string_wrapper::fixed_string<Storage>& u, uint32_t depth ) {
       depth++;
       unsigned_int size;
       fc::raw::unpack( s, size, depth );
       if( size.value > 0 ) {
          if( size.value > sizeof(Storage) ) {
             s.read( (char*)&u.data, sizeof(Storage) );
             char buf[1024];
             size_t left = size.value - sizeof(Storage);
             while( left >= 1024 )
             {
                s.read( buf, 1024 );
                left -= 1024;
             }
             s.read( buf, left );

             /*
             s.seekp( s.tellp() + (size.value - sizeof(Storage)) );
             char tmp;
             size.value -= sizeof(storage);
             while( size.value ){ s.read( &tmp, 1 ); --size.value; }
             */
           //  s.skip( size.value - sizeof(Storage) );
          } else {
             s.read( (char*)&u.data, size.value );
          }
       }
    }

    /*
    template<typename Stream, typename... Args>
    inline void pack( Stream& s, const boost::multiprecision::number<Args...>& d ) {
       s.write( (const char*)&d, sizeof(d) );
    }

    template<typename Stream, typename... Args>
    inline void unpack( Stream& s, boost::multiprecision::number<Args...>& u ) {
       s.read( (const char*)&u, sizeof(u) );
    }
    */
  }
} }

#include <fc/variant.hpp>
namespace fc {
   template<typename Storage>
   void to_variant( const fixed_string<Storage>& s, variant& v ) {
      v = std::string(s);
   }

   template<typename Storage>
   void from_variant( const variant& v, fixed_string<Storage>& s ) {
      s = v.as_string();
   }

template< typename Storage >
struct get_typename< fixed_string< Storage > >
{
   static const char* name()
   {
      static std::string n = std::string("fc::fixed_string_wrapper::fixed_string<")
         + std::to_string( sizeof(Storage) ) + std::string(">");
      return n.c_str();
   }
};

}
