#include <fc/network/http/processors/http_unsupported.hpp>

namespace fc { namespace http {

  namespace detail {

    processor_default::processor_default() {}
    processor_default::~processor_default() {}

    version processor_default::get_version()const
    {
      return version::http_unsupported;
    }

    std::string processor_default::parse_request( const request& r )const
    {
      return "";
    }

    request processor_default::parse_request( const std::string& r )const
    {
      return request{};
    }

    std::string processor_default::parse_response( const response& r )const
    {
      return "";
    }

    response processor_default::parse_response( const std::string& r )const
    {
      return response{};
    }

  } // detail

} } // fc::http