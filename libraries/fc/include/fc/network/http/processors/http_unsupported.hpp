#include <fc/network/http/processor.hpp>

namespace fc { namespace http {

  namespace detail {
    class processor_default final : public processor
    {
    private:
      friend class processor;

      /// Use processor::get_for_version
      processor_default();

    public:
      virtual ~processor_default();

      virtual version get_version()const override;
    };

    // request
    template<> std::string to_string_impl< version::http_unsupported, request >( const request& other );
    template<> request from_string_impl< version::http_unsupported, request >( const std::string& str );

    // response
    template<> std::string to_string_impl< version::http_unsupported, response >( const response& other );
    template<> response from_string_impl< version::http_unsupported, response >( const std::string& str );

  } // detail

} } // fc::http