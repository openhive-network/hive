#pragma once
#include <cstdint>
#include <fc/uint128.hpp>
#include <boost/container/flat_set.hpp>

namespace fc {
class variant;
} // fc

namespace hive { namespace protocol {
template< typename Storage = fc::uint128 >
class fixed_string_impl;

class asset_symbol_type;
class legacy_hive_asset_symbol_type;
struct legacy_hive_asset;
} } // hive::protocol

using boost::container::flat_set;

template< class Key >
class flat_set_ex: public flat_set< Key >
{
  public:

    flat_set_ex& operator=( const flat_set< Key >& obj )
    {
      flat_set< Key >::operator=( obj );
      return *this;
    }

    flat_set_ex& operator=( const flat_set_ex& obj )
    {
      flat_set< Key >::operator=( obj );
      return *this;
    }
};

namespace fc { namespace raw {

template<typename Stream, typename T>
void pack( Stream& s, const flat_set_ex<T>& value );
template<typename Stream, typename T>
void unpack( Stream& s, flat_set_ex<T>& value, uint32_t depth = 0 );

template< typename Stream, typename Storage >
inline void pack( Stream& s, const hive::protocol::fixed_string_impl< Storage >& u );
template< typename Stream, typename Storage >
inline void unpack( Stream& s, hive::protocol::fixed_string_impl< Storage >& u, uint32_t depth = 0 );

template< typename Stream >
inline void pack( Stream& s, const hive::protocol::asset_symbol_type& sym );
template< typename Stream >
inline void unpack( Stream& s, hive::protocol::asset_symbol_type& sym, uint32_t depth = 0 );

template< typename Stream >
inline void pack( Stream& s, const hive::protocol::legacy_hive_asset_symbol_type& sym );
template< typename Stream >
inline void unpack( Stream& s, hive::protocol::legacy_hive_asset_symbol_type& sym, uint32_t depth = 0 );

} // raw

template<typename T>
void to_variant( const flat_set_ex<T>& var,  variant& vo );

template<typename T>
void from_variant( const variant& var, flat_set_ex<T>& vo );

inline void to_variant( const hive::protocol::asset_symbol_type& sym, fc::variant& v );

inline void from_variant( const fc::variant& v, hive::protocol::legacy_hive_asset& leg );
inline void to_variant( const hive::protocol::legacy_hive_asset& leg, fc::variant& v );

template<typename T> struct get_typename<flat_set_ex<T>>
{
  static const char* name()  {
    static std::string n = std::string("flat_set<") + get_typename< fc::flat_set<T> >::name() + ">";
    return n.c_str();
  }
};

} // fc
