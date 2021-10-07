#include <fc/network/http/processors/http_1_1.hpp>

namespace fc { namespace http {

  namespace detail {

    processor_1_1::processor_1_1() {}
    processor_1_1::~processor_1_1() {}

    version processor_1_1::get_version()const
    {
      return version::http_1_1;
    }

  } // detail

} } // fc::http