/*
 * Generator of the description of operations - see operations_description_generation.cmake.
 *
 * Writes the structure of operations and of the types they use - plain objects with basic bounds, independent of the
 * JSON forms in which hived serializes them:
 *
 *   {
 *     "operations": [ {"name": "transfer", "id": 2, "virtual": false, "value": {<schema of the operation>}}, ... ],
 *     "types": { "authority": {<schema of a struct used by operations>}, ... }
 *   }
 *
 * Members are described by JSON schema with `x-hive-type` (names of field types of hiveio_api / schemas) and with
 * hive specific nodes:
 *
 *   - {"x-hive-type": "asset", "x-hive-asset-symbols": ["HIVE", "HBD"]} - an asset of one of the symbols,
 *   - {"x-hive-variant": [null, {"$ref": "#/types/update_proposal_end_date"}]} - one of the types, by their index in
 *     the variant (null - a placeholder for future types, never used).
 *
 * Defaults are given by member initializers or constructors - of scalar members and assets
 * (`{"amount": 1000, "symbol": "HBD"}`, amount in the smallest units).
 *
 * The description knows nothing about JSON forms of hived (HF26, legacy) - generators of API clients build them, so
 * changes of serialization concern only hived and the generators.
 *
 * The structure comes from FC_REFLECT, what reflection does not say (symbols allowed in assets, members whose empty
 * value is allowed) from pure annotations (hive/protocol/annotations.hpp).
 */
#include <hive/protocol/annotations.hpp>
#include <hive/protocol/operations.hpp>

#include <fc/io/json.hpp>
#include <fc/variant_object.hpp>

#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <new>
#include <set>
#include <string>
#include <type_traits>
#include <typeindex>
#include <vector>

namespace {

using namespace hive::protocol;
namespace annotations = hive::protocol::annotations;

template< typename T > constexpr bool dependent_false = false;

// --- type traits ---

template< typename T > struct is_optional : std::false_type {};
template< typename T > struct is_optional< fc::optional< T > > : std::true_type { using type = T; };

template< typename T > struct is_static_variant : std::false_type {};
template< typename... Ts > struct is_static_variant< fc::static_variant< Ts... > > : std::true_type {};

template< typename T > struct is_tiny_asset : std::false_type {};
template< uint32_t SYMBOL > struct is_tiny_asset< tiny_asset< SYMBOL > > : std::true_type
{
  static constexpr uint32_t symbol = SYMBOL;
};

template< typename T, typename = void > struct is_map_like : std::false_type {};
template< typename T > struct is_map_like< T, std::void_t< typename T::key_type, typename T::mapped_type > > : std::true_type {};

template< typename T, typename = void > struct is_sequence : std::false_type {};
template< typename T > struct is_sequence< T, std::void_t< typename T::value_type, decltype( std::declval< T >().begin() ) > >
  : std::bool_constant< !is_map_like< T >::value > {};

template< typename T, typename = void > struct is_sequence_of_assets : std::false_type {};
template< typename T > struct is_sequence_of_assets< T, std::enable_if_t< is_sequence< T >::value > >
  : std::is_same< typename T::value_type, asset > {};

template< typename T > using unwrap_optional_t = typename std::conditional_t< is_optional< T >::value, is_optional< T >, std::type_identity< T > >::type;

// --- values ---

template< typename T >
fc::variant serialized( const T& value )
{
  // the same conversion as in serialization of reflected members (fc::to_variant of bool gives an integer)
  return fc::variant( value );
}

bool same_json( const fc::variant& a, const fc::variant& b )
{
  // fc::variant equality compares only scalars
  return fc::json::to_string( a ) == fc::json::to_string( b );
}

/// Two default constructed instances of T, in memory filled with zeros and with ones - members which differ between them
/// are not initialized by the default constructor, so they have no default value.
template< typename T >
class default_instances
{
  public:
    default_instances()
    {
      std::memset( zeros_storage, 0x00, sizeof( T ) );
      std::memset( ones_storage, 0xff, sizeof( T ) );
      zeros = new( zeros_storage ) T;
      ones = new( ones_storage ) T;
    }

    ~default_instances()
    {
      zeros->~T();
      ones->~T();
    }

    const T* zeros;
    const T* ones;

  private:
    alignas( T ) unsigned char zeros_storage[ sizeof( T ) ];
    alignas( T ) unsigned char ones_storage[ sizeof( T ) ];
};

// --- JSON schema helpers ---

fc::variant hive_type( const char* json_type, const std::string& hive_type_name )
{
  return fc::mutable_variant_object( "type", json_type )( "x-hive-type", hive_type_name );
}

template< typename Integer >
fc::variant integer( const char* hive_type_name )
{
  return fc::mutable_variant_object( "type", "integer" )( "x-hive-type", hive_type_name )
    ( "minimum", int64_t( std::numeric_limits< Integer >::min() ) )
    ( "maximum", int64_t( std::numeric_limits< Integer >::max() ) );
}

fc::variant integer_or_string( const char* hive_type_name )
{
  return fc::mutable_variant_object( "type", fc::variants{ "integer", "string" } )( "x-hive-type", hive_type_name );
}

fc::variant array_of( const fc::variant& items )
{
  return fc::mutable_variant_object( "type", "array" )( "items", items );
}

fc::variant type_ref( const std::string& type_name )
{
  return fc::mutable_variant_object( "$ref", "#/types/" + type_name );
}

std::string without_namespace( const std::string& type_name )
{
  return type_name.substr( type_name.find_last_of( ':' ) + 1 );
}

fc::variant asset_schema( uint8_t symbols )
{
  fc::variants names;
  if( symbols & annotations::HIVE ) names.push_back( "HIVE" );
  if( symbols & annotations::HBD ) names.push_back( "HBD" );
  if( symbols & annotations::VESTS ) names.push_back( "VESTS" );
  return fc::mutable_variant_object( "x-hive-type", "asset" )( "x-hive-asset-symbols", names );
}

template< uint32_t SYMBOL >
uint8_t tiny_asset_symbols()
{
  if constexpr( SYMBOL == HIVE_ASSET_NUM_HIVE ) return annotations::HIVE;
  else if constexpr( SYMBOL == HIVE_ASSET_NUM_HBD ) return annotations::HBD;
  else if constexpr( SYMBOL == HIVE_ASSET_NUM_VESTS ) return annotations::VESTS;
  else static_assert( SYMBOL == HIVE_ASSET_NUM_HIVE, "unknown asset symbol" );
}

std::string symbol_name( const asset_symbol_type& symbol )
{
  switch( symbol.asset_num )
  {
    case HIVE_ASSET_NUM_HIVE: return "HIVE";
    case HIVE_ASSET_NUM_HBD: return "HBD";
    case HIVE_ASSET_NUM_VESTS: return "VESTS";
  }
  FC_THROW( "Unknown asset symbol ${symbol}", ( "symbol", symbol.asset_num ) );
}

fc::variant asset_value( const asset& value )
{
  return fc::mutable_variant_object( "amount", value.amount.value )( "symbol", symbol_name( value.symbol ) );
}

/// Value of a member in the description - assets as amount and symbol, other scalars as serialized by hived. Null for
/// structs, containers and variants, whose defaults are given by their types.
template< typename T >
fc::variant description_value( const T& value )
{
  if constexpr( std::is_same_v< T, asset > ) return asset_value( value );
  else if constexpr( std::is_same_v< T, legacy_hive_asset > ) return asset_value( asset( value.amount, HIVE_SYMBOL ) );
  else if constexpr( is_tiny_asset< T >::value ) return asset_value( value.to_asset() );
  else
  {
    fc::variant result = serialized( value );
    return result.is_object() || result.is_array() ? fc::variant() : result;
  }
}

template< typename Class >
uint8_t annotated_asset_symbols( const char* member )
{
  if constexpr( annotations::asset_symbols< Class >::annotated )
    for( const auto& annotated : annotations::asset_symbols< Class >::members )
      if( std::strcmp( annotated.name, member ) == 0 )
        return annotated.symbols;
  FC_THROW( "Member ${type}::${member} of type asset needs HIVE_ANNOTATE_ASSET_SYMBOLS",
    ( "type", fc::get_typename< Class >::name() )( "member", member ) );
}

template< typename Class >
bool is_optionally_empty_implicitly( const char* member )
{
  if constexpr( annotations::optionally_empty_implicitly< Class >::annotated )
    for( const char* annotated : annotations::optionally_empty_implicitly< Class >::members )
      if( std::strcmp( annotated, member ) == 0 )
        return true;
  return false;
}

class description_builder
{
  public:
    fc::variants operations;
    std::map< std::string, fc::variant > types;

    void add_operations()
    {
      for( int64_t which = 0; which < operation::count(); ++which )
      {
        operation_visitor visitor{ *this, {}, {} };
        operation op( which, visitor );

        operations.push_back( fc::mutable_variant_object( "name", visitor.name )
          ( "id", which )
          ( "virtual", is_virtual_operation( op ) )
          ( "value", visitor.value ) );
      }
    }

    template< typename T >
    fc::variant schema()
    {
      if constexpr( std::is_same_v< T, bool > ) return fc::mutable_variant_object( "type", "boolean" );
      else if constexpr( std::is_same_v< T, std::string > ) return fc::mutable_variant_object( "type", "string" );
      else if constexpr( std::is_same_v< T, account_name_type > ) return hive_type( "string", "AccountName" );
      else if constexpr( std::is_same_v< T, custom_id_type > ) return hive_type( "string", "CustomIdType" );
      else if constexpr( std::is_same_v< T, json_string > ) return hive_type( "string", "JsonString" );
      else if constexpr( std::is_same_v< T, public_key_type > ) return hive_type( "string", "PublicKey" );
      else if constexpr( std::is_same_v< T, signature_type > ) return hive_type( "string", "Signature" );
      else if constexpr( std::is_same_v< T, fc::sha256 > ) return hive_type( "string", "Sha256" );
      else if constexpr( std::is_same_v< T, fc::ripemd160 > ) return hive_type( "string", "BlockId" );
      else if constexpr( std::is_same_v< T, std::vector< char > > ) return hive_type( "string", "Hex" );
      else if constexpr( std::is_same_v< T, fc::time_point_sec > )
        return fc::mutable_variant_object( "type", "string" )( "format", "date-time" );
      else if constexpr( std::is_same_v< T, uint8_t > ) return integer< uint8_t >( "Uint8t" );
      else if constexpr( std::is_same_v< T, uint16_t > ) return integer< uint16_t >( "Uint16t" );
      else if constexpr( std::is_same_v< T, uint32_t > ) return integer< uint32_t >( "Uint32t" );
      else if constexpr( std::is_same_v< T, int16_t > ) return integer< int16_t >( "Int16t" );
      else if constexpr( std::is_same_v< T, int64_t > || std::is_same_v< T, share_type > ) return integer_or_string( "Int64t" );
      else if constexpr( std::is_same_v< T, uint64_t > || std::is_same_v< T, ushare_type > ) return integer_or_string( "Uint64t" );
      else if constexpr( is_tiny_asset< T >::value ) return asset_schema( tiny_asset_symbols< is_tiny_asset< T >::symbol >() );
      else if constexpr( std::is_same_v< T, legacy_hive_asset > ) return asset_schema( annotations::HIVE );
      else if constexpr( std::is_same_v< T, asset > )
        static_assert( dependent_false< T >, "asset is described only as an annotated member" );
      else if constexpr( is_optional< T >::value ) return schema< typename is_optional< T >::type >();
      else if constexpr( is_static_variant< T >::value ) return variant_schema< T >();
      else if constexpr( is_map_like< T >::value )
        return array_of( fc::mutable_variant_object( "type", "array" )
          ( "prefixItems", fc::variants{ schema< typename T::key_type >(), schema< typename T::mapped_type >() } )
          ( "minItems", 2 )( "maxItems", 2 ) );
      else if constexpr( is_sequence< T >::value ) return array_of( schema< typename T::value_type >() );
      else if constexpr( fc::reflector< T >::is_defined::value && !fc::reflector< T >::is_enum::value )
        return type_ref( struct_type< T >() );
      else static_assert( dependent_false< T >, "type not supported by the operations schema generator" );
    }

  private:
    std::map< std::type_index, std::string > type_names;

    struct operation_visitor
    {
      description_builder& builder;
      mutable std::string name;
      mutable fc::variant value;

      typedef void result_type;

      template< typename Operation >
      void operator()( const Operation& ) const
      {
        const std::string type_name = without_namespace( fc::get_typename< Operation >::name() );
        const std::string suffix = "_operation";
        FC_ASSERT( type_name.size() > suffix.size() && type_name.compare( type_name.size() - suffix.size(), suffix.size(), suffix ) == 0,
          "Operation type ${type} does not end with _operation", ( "type", type_name ) );
        name = type_name.substr( 0, type_name.size() - suffix.size() );
        value = builder.struct_schema< Operation >();
      }
    };

    template< typename Class, typename Member >
    fc::variant member_schema( const char* name )
    {
      using T = unwrap_optional_t< Member >;
      if constexpr( std::is_same_v< T, asset > )
        return asset_schema( annotated_asset_symbols< Class >( name ) );
      else if constexpr( is_sequence_of_assets< T >::value )
        return array_of( asset_schema( annotated_asset_symbols< Class >( name ) ) );
      else if constexpr( std::is_same_v< T, account_name_type > )
        return hive_type( "string", is_optionally_empty_implicitly< Class >( name ) ? "OptionallyEmptyAccountName" : "AccountName" );
      else
      {
        FC_ASSERT( !is_optionally_empty_implicitly< Class >( name ),
          "HIVE_ANNOTATE_OPTIONALLY_EMPTY_IMPLICITLY is not supported for ${type}::${member}",
          ( "type", fc::get_typename< Class >::name() )( "member", name ) );
        return schema< T >();
      }
    }

    template< typename Class >
    struct members_visitor
    {
      description_builder& builder;
      const default_instances< Class >& defaults;
      mutable fc::mutable_variant_object properties;
      mutable fc::variants required;

      template< typename Member, typename Owner >
      void operator()( Member Owner::* member, const char* name ) const
      {
        fc::mutable_variant_object property( builder.member_schema< Class, Member >( name ).get_object() );
        if constexpr( is_optional< Member >::value )
          ; // optional members are left out when they are not set
        else
        {
          required.push_back( name );
          // defaults given by member initializers or constructors - neither value initialized nor uninitialized members
          const fc::variant value = description_value( defaults.zeros->*member );
          if( !value.is_null() && same_json( value, description_value( defaults.ones->*member ) )
              && !same_json( value, description_value( Member() ) ) )
            property( "default", value );
          else if( value.is_null() )
            FC_ASSERT( same_json( serialized( defaults.zeros->*member ), serialized( Member() ) ),
              "Default of ${type}::${member} differs from the default of its type - not supported",
              ( "type", fc::get_typename< Class >::name() )( "member", name ) );
        }
        properties( name, property );
      }
    };

    /// JSON schema of a reflected struct
    template< typename T >
    fc::variant struct_schema()
    {
      const default_instances< T > defaults;
      members_visitor< T > visitor{ *this, defaults, {}, {} };
      fc::reflector< T >::visit( visitor );
      fc::mutable_variant_object result( "type", "object" );
      result( "properties", visitor.properties );
      result( "additionalProperties", false );
      if( !visitor.required.empty() )
        result( "required", visitor.required );
      return result;
    }

    /// Name of the reflected struct in `types` - the struct is described there once
    template< typename T >
    std::string struct_type()
    {
      const auto known = type_names.find( typeid( T ) );
      if( known != type_names.end() )
        return known->second;

      const std::string name = without_namespace( fc::get_typename< T >::name() );
      for( const auto& [ type, existing ] : type_names )
        FC_ASSERT( existing != name, "Two types are named ${name}", ( "name", name ) );
      type_names[ typeid( T ) ] = name; // guards against recursion
      types[ name ] = struct_schema< T >();
      return name;
    }

    struct alternative_visitor
    {
      description_builder& builder;
      mutable std::string type_name;

      typedef void result_type;

      template< typename Alternative >
      void operator()( const Alternative& ) const
      {
        if constexpr( !std::is_same_v< Alternative, hive::void_t > )
          type_name = builder.struct_type< Alternative >();
      }
    };

    template< typename Variant >
    fc::variant variant_schema()
    {
      fc::variants alternatives;
      for( int64_t which = 0; which < Variant::count(); ++which )
      {
        alternative_visitor visitor{ *this, {} };
        Variant variant( which, visitor );
        // void_t - a placeholder for future types, never used
        alternatives.push_back( visitor.type_name.empty() ? fc::variant() : type_ref( visitor.type_name ) );
      }
      return fc::mutable_variant_object( "x-hive-variant", alternatives );
    }
};

} // namespace

int main( int argc, char** argv )
{
  try
  {
    FC_ASSERT( argc == 2, "Usage: ${program} <output file>", ( "program", argv[ 0 ] ) );

    description_builder builder;
    builder.add_operations();

    fc::mutable_variant_object types;
    for( const auto& [ name, schema ] : builder.types )
      types( name, schema );

    std::ofstream output( argv[ 1 ] );
    output << fc::json::to_pretty_string( fc::mutable_variant_object( "operations", builder.operations )( "types", types ) )
           << std::endl;
    return output ? 0 : 1;
  }
  catch( const fc::exception& e )
  {
    std::cerr << e.to_detail_string() << std::endl;
    return 1;
  }
}
