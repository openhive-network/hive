#pragma once

#include <fc/uint128.hpp>
#include <fc/io/raw_fwd.hpp>
#include <fc/fixed_string.hpp>

#include <boost/endian/conversion.hpp>
#include <boost/utility/identity_type.hpp>

#include <hive/protocol/types_fwd.hpp>

// These overloads need to be defined before the implementation in fixed_string
namespace fc
{
  /**
    * Endian-reversible pair class.
    */

  template< typename A, typename B >
  struct erpair
  {
    erpair() {}
    erpair(const A& a, const B& b)
      : first(a), second(b) {}
    friend bool operator <  ( const erpair& a, const erpair& b )
    { return std::tie( a.first, a.second ) <  std::tie( b.first, b.second ); }
    friend bool operator <= ( const erpair& a, const erpair& b )
    { return std::tie( a.first, a.second ) <= std::tie( b.first, b.second ); }
    friend bool operator >  ( const erpair& a, const erpair& b )
    { return std::tie( a.first, a.second ) >  std::tie( b.first, b.second ); }
    friend bool operator >= ( const erpair& a, const erpair& b )
    { return std::tie( a.first, a.second ) >= std::tie( b.first, b.second ); }
    friend bool operator == ( const erpair& a, const erpair& b )
    { return std::tie( a.first, a.second ) == std::tie( b.first, b.second ); }
    friend bool operator != ( const erpair& a, const erpair& b )
    { return std::tie( a.first, a.second ) != std::tie( b.first, b.second ); }

    A first{};
    B second{};
  };

  template< typename A, typename B >
  erpair<A, B> make_erpair( const A& a, const B& b )
  {  return erpair<A, B>(a, b); }

  template< typename T >
  T endian_reverse( const T& x )
  {  return boost::endian::endian_reverse(x);  }

  inline uint128 endian_reverse( const uint128& u )
  {  return uint128( boost::endian::endian_reverse( u.hi ), boost::endian::endian_reverse( u.lo ) );  }

  template<typename A, typename B>
  erpair< A, B > endian_reverse( const erpair< A, B >& p )
  {
    return make_erpair( endian_reverse( p.first ), endian_reverse( p.second ) );
  }
}

namespace hive { namespace protocol {

/**
  * This class is an in-place memory allocation of a fixed length character string.
  *
  * The string will serialize the same way as std::string for variant and raw formats.
  */

template< typename _Storage >
class fixed_string_impl
{
  public:
    typedef _Storage Storage;

    fixed_string_impl() = default;
    fixed_string_impl( const fixed_string_impl& c ) : data( c.data ){}
    fixed_string_impl( const char* str ) : fixed_string_impl( std::string( str ) ) {}
    fixed_string_impl( const std::string& str )
    {
      Storage d;

      assign(d, str);

      data = boost::endian::big_to_native( d );
    }

    operator std::string()const
    {
      Storage d = boost::endian::native_to_big( data );
      size_t s = strnlen( (const char*)&d, sizeof(d) );
      const char* self = (const char*)&d;

      return std::string( self, self + s );
    }

    uint32_t size()const
    {
      Storage d = boost::endian::native_to_big( data );

      return strnlen( (const char*)&d, sizeof(d) );
    }

    uint32_t length()const { return size(); }

    fixed_string_impl& operator = ( const fixed_string_impl& str )
    {
      data = str.data;
      return *this;
    }

    fixed_string_impl& operator = ( const char* str )
    {
      *this = fixed_string_impl( str );
      return *this;
    }

    fixed_string_impl& operator = ( const std::string& str )
    {
      *this = fixed_string_impl( str );
      return *this;
    }

    friend std::string operator + ( const fixed_string_impl& a, const std::string& b ) { return std::string( a ) + b; }
    friend std::string operator + ( const std::string& a, const fixed_string_impl& b ){ return a + std::string( b ); }
    friend bool operator < ( const fixed_string_impl& a, const fixed_string_impl& b ) { return a.data < b.data; }
    friend bool operator <= ( const fixed_string_impl& a, const fixed_string_impl& b ) { return a.data <= b.data; }
    friend bool operator > ( const fixed_string_impl& a, const fixed_string_impl& b ) { return a.data > b.data; }
    friend bool operator >= ( const fixed_string_impl& a, const fixed_string_impl& b ) { return a.data >= b.data; }
    friend bool operator == ( const fixed_string_impl& a, const fixed_string_impl& b ) { return a.data == b.data; }
    friend bool operator != ( const fixed_string_impl& a, const fixed_string_impl& b ) { return a.data != b.data; }

    Storage data;

  private:
    void assign(Storage& storage, const char* in, size_t in_len) const
    {
      if( fc::fixed_string_wrapper::verifier_switch::is_verifying_enabled() )
      {
        FC_ASSERT(in_len <= sizeof(data), "Input too large: `${in}` (${is}) for fixed size string: (${fs})", (in)("is", in_len)("fs", sizeof(data)));
        memcpy( (char*)&storage, in, in_len );
      }
      else
      {
        if( in_len <= sizeof(storage) )
          memcpy( (char*)&storage, in, in_len );
        else
          memcpy( (char*)&storage, in, sizeof(storage) );
      }
    }

    void assign(Storage& storage, const std::string& in) const
    {
      assign(storage, in.c_str(), in.size());
    }
};

// These storage types work with memory layout and should be used instead of a custom template.
template< size_t N > struct fixed_string_impl_for_size;
template< typename T > struct fixed_string_size_for_impl;

//
// The main purpose of this macro is to auto-generate the boilerplate
// for different fixed_string sizes that have different backing types.
//
// This "boilerplate" is simply templates which allow us to go from
// size -> type and from type -> size at compile time.
//
// We want to write this one-liner:
//
//     HIVE_DEFINE_FIXED_STRING_IMPL( 32, fc::erpair< fc::uint128_t, fc::uint128_t > )
//
// Unfortunately, since the preprocessor doesn't do syntactic parsing,
// this would be regarded as a 3-argument call (since there are three commas,
// and the PP doesn't "know" enough C++ syntax to "understand" the internal
// comma separating the two arguments to erpair "shouldn't" be taken
// as delimiting separate macro arguments).
//
// Fortunately, there is a solution that involves using parentheses to "quote"
// the argument, then using a dummy function type to "unquote" it.
// This solution is explained here:
//
//     https://stackoverflow.com/questions/13842468/comma-in-c-c-macro
//
// We don't have to code this preprocessor hack ourselves, as a battle-tested,
// widely-compatible implementation is available in the Boost Identity Type
// library.  Which allows our one-liner to be:
//
//     HIVE_DEFINE_FIXED_STRING_IMPL( 32, BOOST_IDENTITY_TYPE((fc::erpair< fc::uint128_t, fc::uint128_t >)) )
//
#define HIVE_DEFINE_FIXED_STRING_IMPL( SIZE, STORAGE_TYPE )       \
template<>                                                         \
struct fixed_string_impl_for_size< SIZE >                          \
{                                                                  \
  typedef hive::protocol::fixed_string_impl< STORAGE_TYPE > t;   \
};                                                                 \
                                              \
template<>                                                         \
struct fixed_string_size_for_impl< STORAGE_TYPE >                  \
{                                                                  \
  static const size_t size = SIZE;                                \
};

HIVE_DEFINE_FIXED_STRING_IMPL( 16, BOOST_IDENTITY_TYPE((fc::uint128_t)) )
HIVE_DEFINE_FIXED_STRING_IMPL( 24, BOOST_IDENTITY_TYPE((fc::erpair< fc::uint128_t, uint64_t >)) )
HIVE_DEFINE_FIXED_STRING_IMPL( 32, BOOST_IDENTITY_TYPE((fc::erpair< fc::uint128_t, fc::uint128_t >)) )

template< size_t N >
using fixed_string = typename fixed_string_impl_for_size<N>::t;

} } // hive::protocol

namespace fc { namespace raw {

template< typename Stream, typename Storage >
inline void pack( Stream& s, const hive::protocol::fixed_string_impl< Storage >& u )
{
  pack( s, std::string( u ) );
}

template< typename Stream, typename Storage >
inline void unpack( Stream& s, hive::protocol::fixed_string_impl< Storage >& u, uint32_t depth )
{
  depth++;
  std::string str;
  unpack( s, str, depth );
  u = str;
}

} // raw

template< typename Storage >
void to_variant(   const hive::protocol::fixed_string_impl< Storage >& s, variant& v )
{
  v = std::string( s );
}

template< typename Storage >
void from_variant( const variant& v, hive::protocol::fixed_string_impl< Storage >& s )
{
  s = v.as_string();
}

template< typename Storage >
struct get_typename< hive::protocol::fixed_string_impl< Storage > >
{
  static const char* name()
  {
    static const std::string n =
      std::string("hive::protocol::fixed_string<") +
      std::to_string( hive::protocol::fixed_string_size_for_impl<Storage>::size ) +
      std::string(">");
    return n.c_str();
  }
};

} // fc
