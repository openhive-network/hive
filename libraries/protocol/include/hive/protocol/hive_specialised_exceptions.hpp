#pragma once

#include <fc/exception/exception.hpp>

#define _HA_GET_FORMAT( FORMAT, ...) FORMAT
#define _HA_GET_VARGS( FORMAT, ...) __VA_ARGS__

#define HIVE_SPECIALISED_ASSERT( expr, category, subject_type, expr_str, ... )                        \
  FC_EXPAND_MACRO(                                                                               \
    FC_MULTILINE_MACRO_BEGIN                                                                     \
      if( UNLIKELY(!(expr)) )                                                                    \
      {                                                                                          \
        FC_ASSERT_EXCEPTION_DECL(                                                                \
          expr_str,                                                                              \
          fc::assert_exception,                                                                  \
          _HA_GET_FORMAT( "" __VA_ARGS__ ),                                                      \
          _HA_GET_VARGS( "" __VA_ARGS__ )("category", category )("subject_type", subject_type )  \
        );                                                                                       \
        if( fc::enable_record_assert_trip )                                                      \
          FC_RECORD_ASSERT( __e__ );                                                             \
        throw __e__;                                                                             \
      }                                                                                          \
    FC_MULTILINE_MACRO_END                                                                       \
  )

#define HIVE_SPECIALISED_ASSERT_WITH_SUBJECT( expr, category, subject_type, subject, ... )                  \
  FC_EXPAND_MACRO(                                                                                                    \
    FC_MULTILINE_MACRO_BEGIN                                                                                          \
      if( UNLIKELY(!(expr)) )                                                                                         \
      {                                                                                                               \
        FC_ASSERT_EXCEPTION_DECL(                                                                                     \
          #expr,                                                                                                   \
          fc::assert_exception,                                                                                       \
          _HA_GET_FORMAT( "" __VA_ARGS__ ),                                                                           \
          _HA_GET_VARGS( "" __VA_ARGS__ )("category", category )("subject_type", subject_type )("subject", subject)   \
        );                                                                                                            \
        if( fc::enable_record_assert_trip )                                                                           \
          FC_RECORD_ASSERT( __e__ );                                                                                  \
        throw __e__;                                                                                                  \
      }                                                                                                               \
    FC_MULTILINE_MACRO_END                                                                                            \
  )


#define HIVE_PROTOCOL_ASSET_ASSERT( expr, ... )           HIVE_SPECIALISED_ASSERT( expr, "protocol", "asset",      #expr, __VA_ARGS__ );
#define HIVE_PROTOCOL_AUTHORITY_ASSERT( expr, ... )       HIVE_SPECIALISED_ASSERT( expr, "protocol", "authority",  #expr, __VA_ARGS__ );
#define HIVE_PROTOCOL_OPERATIONS_ASSERT( expr, ... )      HIVE_SPECIALISED_ASSERT( expr, "protocol", "operation",  #expr, __VA_ARGS__ );
#define HIVE_PROTOCOL_VALIDATION_ASSERT( expr, ... )      HIVE_SPECIALISED_ASSERT( expr, "protocol", "any", #expr, __VA_ARGS__ );
#define HIVE_PROTOCOL_ACCOUNT_NAME_ASSERT( expr, ... )    HIVE_SPECIALISED_ASSERT( expr, "protocol", "account_name", #expr, __VA_ARGS__ );
#define HIVE_PROTOCOL_HARDFORK_ASSERT( expr, ... )        HIVE_SPECIALISED_ASSERT( expr, "protocol", "hardfork",   #expr, __VA_ARGS__ );
