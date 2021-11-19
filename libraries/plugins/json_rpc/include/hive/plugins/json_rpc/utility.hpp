#pragma once

#include <type_traits>

#include <fc/reflect/reflect.hpp>
#include <fc/macros.hpp>

#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/cat.hpp>

#define DECLARE_API_METHOD_HELPER( r, data, method ) \
BOOST_PP_CAT( method, _return ) method( const BOOST_PP_CAT( method, _args )& args, bool lock = false );

#define FOR_EACH_API_HELPER( r, callback, method ) \
{ \
  typedef std::remove_pointer<decltype(this)>::type this_type; \
  \
  callback( \
    (*this), \
    BOOST_PP_STRINGIZE( method ), \
    &this_type::method, \
    static_cast< BOOST_PP_CAT( method, _args )* >(nullptr), \
    static_cast< BOOST_PP_CAT( method, _return )* >(nullptr) \
  ); \
}

#define DECLARE_API( METHODS ) \
  BOOST_PP_SEQ_FOR_EACH( DECLARE_API_METHOD_HELPER, _, METHODS ) \
  \
  template< typename Lambda > \
  void for_each_api( Lambda&& callback ) \
  { \
    BOOST_PP_SEQ_FOR_EACH( FOR_EACH_API_HELPER, callback, METHODS ) \
  }

#define DECLARE_API_IMPL_HELPER( r, data, method ) \
BOOST_PP_CAT( method, _return ) method( const BOOST_PP_CAT( method, _args )& args );

#define DECLARE_API_IMPL( METHODS ) \
BOOST_PP_SEQ_FOR_EACH( DECLARE_API_IMPL_HELPER, _, METHODS )

#define DEFINE_API_IMPL( class, method )                                                        \
BOOST_PP_CAT( method, _return ) class :: method ( const BOOST_PP_CAT( method, _args )& args )   \

#define DEFINE_READ_API_HELPER( r, class, method )                                                       \
BOOST_PP_CAT( method, _return ) class :: method ( const BOOST_PP_CAT( method, _args )& args, bool lock ) \
{                                                                                                        \
  if( lock )                                                                                            \
  {                                                                                                     \
    return my->_db.with_read_lock( [&args, this](){ return my->method( args ); });                     \
  }                                                                                                     \
  else                                                                                                  \
  {                                                                                                     \
    return my->method( args );                                                                         \
  }                                                                                                     \
}

#define DEFINE_WRITE_API_HELPER( r, class, method )                                                      \
BOOST_PP_CAT( method, _return ) class :: method ( const BOOST_PP_CAT( method, _args )& args, bool lock ) \
{                                                                                                        \
  if( lock )                                                                                            \
  {                                                                                                     \
    return my->_db.with_write_lock( [&args, this](){ return my->method( args ); });                    \
  }                                                                                                     \
  else                                                                                                  \
  {                                                                                                     \
    return my->method( args );                                                                         \
  }                                                                                                     \
}

#define DEFINE_LOCKLESS_API_HELPER( r, class, method )                                                   \
BOOST_PP_CAT( method, _return ) class :: method ( const BOOST_PP_CAT( method, _args )& args, bool lock ) \
{                                                                                                        \
  FC_UNUSED( lock );                                                                                     \
  return my->method( args );                                                                            \
}

#define DEFINE_READ_APIS( class, METHODS ) \
  BOOST_PP_SEQ_FOR_EACH( DEFINE_READ_API_HELPER, class, METHODS )

#define DEFINE_WRITE_APIS( class, METHODS ) \
  BOOST_PP_SEQ_FOR_EACH( DEFINE_WRITE_API_HELPER, class, METHODS )

#define DEFINE_LOCKLESS_APIS( class, METHODS ) \
  BOOST_PP_SEQ_FOR_EACH( DEFINE_LOCKLESS_API_HELPER, class, METHODS )

#define LOG_DELAY_EX(start_time, log_threshold, msg, e) \
  { fc::time_point current_time = fc::time_point::now(); \
    fc::microseconds delay = current_time - start_time; \
    if (delay > log_threshold) { \
      double delay_seconds = (static_cast<double>(delay.count()) / 1000) / 1000.0; \
      std::ostringstream os; \
      os << std::fixed << std::setprecision(3) << delay_seconds; \
      ulog( msg ": ${delay_seconds} s", ("delay_seconds", os.str())e ); \
      } \
  }

#define LOG_DELAY(start_time, log_threshold, msg) \
  LOG_DELAY_EX(start_time, log_threshold, msg,)

namespace hive { namespace plugins { namespace json_rpc {

struct void_type {};

} } } // hive::plugins::json_rpc

FC_REFLECT( hive::plugins::json_rpc::void_type, )
