#include <fc/network/http/processors/http_1_1.hpp>

namespace fc { namespace http {

  namespace detail {

    processor_1_1::processor_1_1() {}
    processor_1_1::~processor_1_1() {}

    version processor_1_1::get_version()const
    {
      return version::http_1_1;
    }

    std::string processor_1_1::parse_request( const request& r )const
    {
      return "";
    }

    request processor_1_1::parse_request( const std::string& r )const
    {
      return request{};
    }

    std::string processor_1_1::parse_response( const response& r )const
    {
      return "";
    }

    response processor_1_1::parse_response( const std::string& r )const
    {
      return response{};
    }

  } // detail

} } // fc::http