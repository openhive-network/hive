#pragma once

#include <fc/exception/exception.hpp>

#define _HA_GET_FORMAT( FORMAT, ...) FORMAT
#define _HA_GET_VARGS( FORMAT, ...) __VA_ARGS__

#define HIVE_SPECIALISED_ASSERT( category, subcategory, expr_str, expr, ... )                    \
  FC_EXPAND_MACRO(                                                                               \
    FC_MULTILINE_MACRO_BEGIN                                                                     \
      if( UNLIKELY(!(expr)) )                                                                    \
      {                                                                                          \
        FC_ASSERT_EXCEPTION_DECL(                                                                \
          expr_str,                                                                              \
          fc::assert_exception,                                                                  \
          _HA_GET_FORMAT( "" __VA_ARGS__ ),                                                      \
          _HA_GET_VARGS( "" __VA_ARGS__ )("category", category )("subcategory", subcategory )    \
        );                                                                                       \
        if( fc::enable_record_assert_trip )                                                      \
          FC_RECORD_ASSERT( __e__ );                                                             \
        throw __e__;                                                                             \
      }                                                                                          \
    FC_MULTILINE_MACRO_END                                                                       \
  )


#define HIVE_PROTOCOL_CRYPTO_ASSERT( expr, ... )        HIVE_SPECIALISED_ASSERT( "protocol", "crypto",     #expr, expr, __VA_ARGS__ );
#define HIVE_PROTOCOL_ASSET_ASSERT( expr, ... )         HIVE_SPECIALISED_ASSERT( "protocol", "asset",      #expr, expr, __VA_ARGS__ );
#define HIVE_PROTOCOL_AUTHORITY_ASSERT( expr, ... )     HIVE_SPECIALISED_ASSERT( "protocol", "authority",  #expr, expr, __VA_ARGS__ );
#define HIVE_PROTOCOL_OPERATIONS_ASSERT( expr, ... )    HIVE_SPECIALISED_ASSERT( "protocol", "operation",  #expr, expr, __VA_ARGS__ );
#define HIVE_PROTOCOL_REWARD_ASSERT( expr, ... )        HIVE_SPECIALISED_ASSERT( "protocol", "reward",     #expr, expr, __VA_ARGS__ );
#define HIVE_PROTOCOL_VALIDATION_ASSERT( expr, ... )    HIVE_SPECIALISED_ASSERT( "protocol", "validation", #expr, expr, __VA_ARGS__ );
#define HIVE_PROTOCOL_HARDFORK_ASSERT( expr, ... )      HIVE_SPECIALISED_ASSERT( "protocol", "hardfork",   #expr, expr, __VA_ARGS__ );
