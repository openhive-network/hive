#pragma once

/**
 * Pure annotations of reflected protocol types.
 *
 * The macros below only attach metadata to types reflected with FC_REFLECT. The metadata is read solely by generators
 * of descriptions of the types (e.g. the description of operations, see cmake/operations_description_generation.cmake).
 * They declare specializations of templates which nothing else uses - they change neither the annotated types nor
 * their serialization, validation or any other behavior, so they have no effect on consensus or on the rest of the
 * code. Everything they contain is evaluated at compile time; checks of annotated members are static_asserts.
 *
 * Usage (next to FC_REFLECT of the type, in the global namespace):
 *
 *   HIVE_ANNOTATE_ASSET_SYMBOLS( hive::protocol::transfer_operation, (amount, HIVE | HBD) )
 *     - symbols allowed in members of type asset (or containers of assets) - `asset` itself accepts any symbol,
 *       the allowed ones are checked by validate(),
 *
 *   HIVE_ANNOTATE_OPTIONALLY_EMPTY_IMPLICITLY( hive::protocol::comment_operation, (parent_author) )
 *     - members whose empty value is allowed and has a meaning (e.g. an empty parent_author of a comment means a
 *       top level post), although their type does not say so (they are not optional<>).
 */

#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/seq/variadic_seq_to_seq.hpp>
#include <boost/preprocessor/stringize.hpp>
#include <boost/preprocessor/tuple/elem.hpp>

#include <cstdint>
#include <type_traits>

namespace hive { namespace protocol { namespace annotations {

/// Asset symbols of HIVE_ANNOTATE_ASSET_SYMBOLS - flags, combined with `|`
enum asset_symbol_flag : uint8_t
{
  HIVE  = 1 << 0,
  HBD   = 1 << 1,
  VESTS = 1 << 2
};

struct asset_symbols_member
{
  const char* name;
  uint8_t     symbols; ///< combination of asset_symbol_flag
};

/// Symbols allowed in asset members of T - specialized by HIVE_ANNOTATE_ASSET_SYMBOLS
template< typename T >
struct asset_symbols
{
  static constexpr bool annotated = false;
};

/// Members of T whose empty value is allowed - specialized by HIVE_ANNOTATE_OPTIONALLY_EMPTY_IMPLICITLY
template< typename T >
struct optionally_empty_implicitly
{
  static constexpr bool annotated = false;
};

} } } // hive::protocol::annotations

#define HIVE_ANNOTATIONS_ASSERT_MEMBER( TYPE, MEMBER ) \
  static_assert( std::is_member_object_pointer_v< decltype( &TYPE::MEMBER ) >, \
    BOOST_PP_STRINGIZE( TYPE ) " has no member " BOOST_PP_STRINGIZE( MEMBER ) );

#define HIVE_ANNOTATIONS_ASSET_SYMBOLS_MEMBER( r, TYPE, ELEM ) \
  { BOOST_PP_STRINGIZE( BOOST_PP_TUPLE_ELEM( 2, 0, ELEM ) ), \
    uint8_t( BOOST_PP_TUPLE_ELEM( 2, 1, ELEM ) ) },

#define HIVE_ANNOTATIONS_ASSET_SYMBOLS_ASSERT( r, TYPE, ELEM ) \
  HIVE_ANNOTATIONS_ASSERT_MEMBER( TYPE, BOOST_PP_TUPLE_ELEM( 2, 0, ELEM ) )

/// See annotations.hpp - a pure annotation, without any effect on the annotated type and the rest of the code.
#define HIVE_ANNOTATE_ASSET_SYMBOLS( TYPE, MEMBERS ) \
namespace hive { namespace protocol { namespace annotations { \
template<> struct asset_symbols< TYPE > \
{ \
  BOOST_PP_SEQ_FOR_EACH( HIVE_ANNOTATIONS_ASSET_SYMBOLS_ASSERT, TYPE, BOOST_PP_VARIADIC_SEQ_TO_SEQ( MEMBERS ) ) \
  static constexpr bool annotated = true; \
  static constexpr asset_symbols_member members[] = { \
    BOOST_PP_SEQ_FOR_EACH( HIVE_ANNOTATIONS_ASSET_SYMBOLS_MEMBER, TYPE, BOOST_PP_VARIADIC_SEQ_TO_SEQ( MEMBERS ) ) \
  }; \
}; \
} } }

#define HIVE_ANNOTATIONS_MEMBER_NAME( r, TYPE, MEMBER ) BOOST_PP_STRINGIZE( MEMBER ),

#define HIVE_ANNOTATIONS_MEMBER_ASSERT( r, TYPE, MEMBER ) HIVE_ANNOTATIONS_ASSERT_MEMBER( TYPE, MEMBER )

/// See annotations.hpp - a pure annotation, without any effect on the annotated type and the rest of the code.
#define HIVE_ANNOTATE_OPTIONALLY_EMPTY_IMPLICITLY( TYPE, MEMBERS ) \
namespace hive { namespace protocol { namespace annotations { \
template<> struct optionally_empty_implicitly< TYPE > \
{ \
  BOOST_PP_SEQ_FOR_EACH( HIVE_ANNOTATIONS_MEMBER_ASSERT, TYPE, MEMBERS ) \
  static constexpr bool annotated = true; \
  static constexpr const char* members[] = { BOOST_PP_SEQ_FOR_EACH( HIVE_ANNOTATIONS_MEMBER_NAME, TYPE, MEMBERS ) }; \
}; \
} } }
