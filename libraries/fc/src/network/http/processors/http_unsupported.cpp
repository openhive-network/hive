#include <fc/network/http/processors/http_unsupported.hpp>

namespace fc { namespace http {

  namespace detail {

    processor_default::processor_default() {}
    processor_default::~processor_default() {}

    version processor_default::get_version()const
    {
      return version::http_unsupported;
    }

  } // detail

} } // fc::http