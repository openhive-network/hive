#include <fc/reflect/typename.hpp>

#include <fc/log/logger.hpp>

namespace fc{

std::string trim_legacy_typename_namespace( const std::string& name )
{
  auto start = name.find_last_of( ':' ) + 1;
  auto end   = name.find_last_of( '_' );
  return name.substr( start, end-start );
}

std::string trim_typename_namespace( const std::string& name )
{
  auto start = name.find_last_of( ':' );
  start = ( start == std::string::npos ) ? 0 : start + 1;
  return name.substr( start );
}

std::string trim_operation( const std::string& name )
{
  auto pos = name.find_last_of( '_' );
  if( pos == std::string::npos )
    pos = name.size();

  return name.substr( 0, pos );
}

}
