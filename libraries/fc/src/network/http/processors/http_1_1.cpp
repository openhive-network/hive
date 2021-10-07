#include <fc/network/http/processors/http_1_1.hpp>

namespace fc { namespace http {

  namespace detail {

    processor_1_1::processor_1_1() {}
    processor_1_1::~processor_1_1() {}

    version processor_1_1::get_version()const
    {
      return version::http_1_1;
    }

    template<>
    std::string to_string_impl< version::http_1_1, request >( const request& other ) { return ""; } // TODO: Implement
    template<>
    request from_string_impl< version::http_1_1, request >( const std::string& str ) { return request{}; } // TODO: Implement

    template<>
    std::string to_string_impl< version::http_1_1, response >( const response& other ) { return ""; } // TODO: Implement
    template<>
    response from_string_impl< version::http_1_1, response >( const std::string& str ) { return response{}; } // TODO: Implement

  } // detail

} } // fc::http