#include <fc/network/http/processors/http_unsupported.hpp>

namespace fc { namespace http {

  namespace detail {

    processor_default::processor_default() {}
    processor_default::~processor_default() {}

    version processor_default::get_version()const
    {
      return version::http_unsupported;
    }

    template<>
    std::string to_string_impl< version::http_unsupported, request >( const request& other ) { return ""; } // TODO: Implement
    template<>
    request from_string_impl< version::http_unsupported, request >( const std::string& str ) { return request{}; } // TODO: Implement

    template<>
    std::string to_string_impl< version::http_unsupported, response >( const response& other ) { return ""; } // TODO: Implement
    template<>
    response from_string_impl< version::http_unsupported, response >( const std::string& str ) { return response{}; } // TODO: Implement

  } // detail

} } // fc::http