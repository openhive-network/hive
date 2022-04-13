
#pragma once

#include <hive/protocol/authority.hpp>
#include <hive/protocol/base.hpp>
#include <hive/protocol/misc_utilities.hpp>

#include <fc/variant.hpp>

#include <boost/container/flat_set.hpp>

#include <string>
#include <vector>

namespace hive { namespace protocol {

struct get_required_auth_visitor
{
  typedef void result_type;

  flat_set< account_name_type >&        active;
  flat_set< account_name_type >&        owner;
  flat_set< account_name_type >&        posting;
  flat_set< account_name_type >&        witness;
  std::vector< authority >&  other;

  get_required_auth_visitor(
      flat_set< account_name_type >& a,
      flat_set< account_name_type >& own,
      flat_set< account_name_type >& post,
      flat_set< account_name_type >& wit,
      std::vector< authority >& oth )
    : active( a ), owner( own ), posting( post ), witness( wit ), other( oth ) {}

  template< typename ...Ts >
  void operator()( const fc::static_variant< Ts... >& v )
  {
    v.visit( *this );
  }

  template< typename T >
  void operator()( const T& v )const
  {
    /// Here GCC 10.1 generates fake warning related to uninitialized variables.
    /// GCC 10.2 has fixed this problem
    v.get_required_active_authorities( active );
    v.get_required_owner_authorities( owner );
    v.get_required_posting_authorities( posting );
    v.get_required_witness_signatures( witness );
    v.get_required_authorities( other );
  }
};

} } // hive::protocol

//
// Place HIVE_DECLARE_OPERATION_TYPE in a .hpp file to declare
// functions related to your operation type
//
#define HIVE_DECLARE_OPERATION_TYPE( OperationType )                            \
                                                      \
namespace hive { namespace protocol {                                           \
                                                      \
void operation_validate( const OperationType& o );                               \
void operation_get_required_authorities( const OperationType& op,                \
                            flat_set< account_name_type >& active,  \
                            flat_set< account_name_type >& owner,   \
                            flat_set< account_name_type >& posting, \
                            flat_set< account_name_type >& witness, \
                            vector< authority >& other );           \
                                                      \
} } /* hive::protocol */

namespace fc
{

struct get_legacy_static_variant_name
{
  string& name;
  get_legacy_static_variant_name( string& dv )
    : name( dv ) {}

  typedef void result_type;
  template< typename T > void operator()( const T& v )const
  {
    name = hive::protocol::trim_legacy_typename_namespace( fc::get_typename< T >::name() );
  }
};

template< typename static_variant >
struct extended_serialization_functor
{
  bool serialize( const fc::variant& v, static_variant& s ) const
  {
    static std::map< string, int64_t > to_legacy_tag = []()
    {
        std::map< string, int64_t > name_map;
        for( int i = 0; i < static_variant::count(); ++i )
        {
          static_variant tmp;
          tmp.set_which(i);
          string n;
          tmp.visit( get_legacy_static_variant_name( n ) );
          name_map[n] = i;
        }
        return name_map;
    }();

    if( !hive::protocol::serialization_mode_controller::legacy_enabled() )
      return false;

    auto ar = v.get_array();
    if( ar.size() < 2 ) return true;
    if( ar[0].is_uint64() )
      s.set_which( ar[0].as_uint64() );
    else
    {
      auto itr = to_legacy_tag.find(ar[0].as_string());
      FC_ASSERT( itr != to_legacy_tag.end(), "Invalid operation name: ${n}", ("n", ar[0]) );
      s.set_which( itr->second );
    }
    s.visit( fc::to_static_variant( ar[1] ) );

    return true;
  }
};

template< typename static_variant >
struct extended_variant_creator_functor
{
  template<typename T>
  fc::variant create( const T& v ) const
  {
    if( hive::protocol::serialization_mode_controller::legacy_enabled() )
    {
      auto name = hive::protocol::trim_legacy_typename_namespace( fc::get_typename< T >::name() );
      return variants( { variant( name ), variant( v ) } );
    }
    else
    {
      auto name = trim_typename_namespace( fc::get_typename< T >::name() );
      return mutable_variant_object( "type", name )( "value", v );
    }
  }
};

}
