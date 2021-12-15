#pragma once

#include <fc/reflect/reflect.hpp>
#include <fc/exception/exception.hpp>
#include <fc/log/logger.hpp>

namespace hive { namespace protocol {

enum curve_id
{
  quadratic,
  bounded_curation,
  linear,
  square_root,
  convergent_linear,
  convergent_square_root
};

struct dynamic_serializer
{
  /*
    This switch is used for switching of serialization.
  */
  thread_local static bool legacy_enabled;
};

struct legacy_switcher
{
  const bool old_legacy_enabled = false;

  static const std::string serialization_detector;

  legacy_switcher( bool val );
  ~legacy_switcher();

  static bool try_another_serialization( const fc::bad_cast_exception& e );
};

struct legacy_manager
{
  template< typename T>
  static void exec( T&& call )
  {
    try
    {
      //Compatibility with older shape of asset
      legacy_switcher switcher( true );
      call();
    }
    catch( fc::bad_cast_exception& e )
    {
      if( legacy_switcher::try_another_serialization( e ) )
      {
        try
        {
          legacy_switcher switcher( false );
          ilog("Change of serialization - a legacy is ${le} now", ( "le", dynamic_serializer::legacy_enabled ? "enabled" : "disabled" ) );
          call();
        } FC_CAPTURE_AND_RETHROW()
      }
    }
    catch(...) 
    {
      try
      {
        auto eptr = std::current_exception();
        std::rethrow_exception( eptr );
      } FC_CAPTURE_AND_RETHROW()
    }
  }
};

std::string trim_legacy_typename_namespace( const std::string& name );

} } // hive::protocol


FC_REFLECT_ENUM(
  hive::protocol::curve_id,
  (quadratic)
  (bounded_curation)
  (linear)
  (square_root)
  (convergent_linear)
  (convergent_square_root)
)
