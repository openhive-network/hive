#include <fc/reflect/typename.hpp>

#include <fc/log/logger.hpp>

namespace fc{

std::string trim_legacy_typename_namespace( const std::string& name )
{
  auto start = name.find_last_of( ':' ) + 1;
  auto end   = name.find_last_of( '_' );

  //An exception for `update_proposal_end_date` operation. Unfortunately this operation hasn't a postix `operation`.
  if( end != std::string::npos )
  {
    const std::string _postfix = "operation";
    if( name.substr( end + 1 ) != _postfix )
      end = name.size();
  }

  return name.substr( start, end-start );
}

std::string trim_typename_namespace( const std::string& name )
{
  auto start = name.find_last_of( ':' );
  start = ( start == std::string::npos ) ? 0 : start + 1;
  return name.substr( start );
}

}
