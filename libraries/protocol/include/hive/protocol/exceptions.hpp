#pragma once

#include <fc/exception/exception.hpp>
#include <hive/protocol/protocol.hpp>

#define HIVE_ASSERT( expr, exc_type, FORMAT, ... )                      \
  FC_EXPAND_MACRO(                                                      \
    FC_MULTILINE_MACRO_BEGIN                                            \
      if( UNLIKELY(!(expr)) )                                           \
      {                                                                 \
        FC_ASSERT_EXCEPTION_DECL( #expr, exc_type, FORMAT, __VA_ARGS__ )\
        if( fc::enable_record_assert_trip )                             \
          FC_RECORD_ASSERT( __e__ );                                    \
        throw __e__;                                                    \
      }                                                                 \
    FC_MULTILINE_MACRO_END                                              \
  )

#define HIVE_FINALIZABLE_ASSERT( expr, exc_type, FORMAT, ... )          \
  FC_EXPAND_MACRO(                                                      \
    FC_MULTILINE_MACRO_BEGIN                                            \
      if( UNLIKELY(!(expr)) )                                           \
      {                                                                 \
        FC_ASSERT_EXCEPTION_DECL( #expr, exc_type, FORMAT, __VA_ARGS__ )\
        throw __e__;                                                    \
      }                                                                 \
    FC_MULTILINE_MACRO_END                                              \
  )

#define HIVE_FINALIZE_ASSERT( ASSERT ) \
  FC_FINALIZE_ASSERT( ASSERT )

namespace hive { namespace protocol {

  FC_DECLARE_DERIVED_EXCEPTION( transaction_exception,             fc::assert_exception, 3000000, "transaction exception" )
  FC_DECLARE_DERIVED_EXCEPTION( transaction_auth_exception,        hive::protocol::transaction_exception, 3070000, "transaction authorization exception" )

  FC_DECLARE_DERIVED_EXCEPTION( tx_missing_active_auth,            hive::protocol::transaction_auth_exception, 3010000, "missing required active authority" )
  FC_DECLARE_DERIVED_EXCEPTION( tx_missing_owner_auth,             hive::protocol::transaction_auth_exception, 3020000, "missing required owner authority" )
  FC_DECLARE_DERIVED_EXCEPTION( tx_missing_posting_auth,           hive::protocol::transaction_auth_exception, 3030000, "missing required posting authority" )
  FC_DECLARE_DERIVED_EXCEPTION( tx_missing_other_auth,             hive::protocol::transaction_auth_exception, 3040000, "missing required other authority" )
  FC_DECLARE_DERIVED_EXCEPTION( tx_irrelevant_sig,                 hive::protocol::transaction_exception, 3050000, "irrelevant signature included" )
  FC_DECLARE_DERIVED_EXCEPTION( tx_duplicate_sig,                  hive::protocol::transaction_exception, 3060000, "duplicate signature included" )
  FC_DECLARE_DERIVED_EXCEPTION( tx_missing_witness_auth,           hive::protocol::transaction_exception, 3070000, "missing required witness signature" )

  #define HIVE_RECODE_EXC( cause_type, effect_type ) \
    catch( const cause_type& e ) \
    { throw( effect_type( e.what(), e.get_log() ) ); }

} } // hive::protocol
