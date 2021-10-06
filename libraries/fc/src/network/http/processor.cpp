#include <fc/network/http/processor.hpp>
#include <fc/exception/exception.hpp>
#include <sstream>

namespace fc { namespace http {

  http_target::http_target( const std::string& str_target )
    : str_target( str_target )
  {
    FC_ASSERT( str_target.size(), "Target should not be empty" );
    switch( str_target.at( 0 ) )
    {
      case '/':
        type = target_type::path;
        break;
      case '*':
        type = target_type::asterisk;
        break;
      default:
        // We assume that only http and https is accepted
        if( str_target.substr( 0, 7 ) == "http://" || str_target.substr( 0, 8 ) == "https://" )
          type = target_type::url;
        else
          type = target_type::authority;
        break;
    }
  }

  const std::string& http_target::str()const
  {
    return str_target;
  }

  target_type http_target::get()const
  {
    return type;
  }


} } // fc::http