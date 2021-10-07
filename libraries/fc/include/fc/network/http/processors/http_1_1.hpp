#include <fc/network/http/processor.hpp>

namespace fc { namespace http {

  namespace detail {
    class processor_1_1 final : public processor
    {
    private:
      friend class processor;

      /// Use processor::get_for_version
      processor_1_1();

    public:
      virtual ~processor_1_1();

      virtual version get_version()const override;
    };

    // request
    template<> std::string to_string_impl< version::http_1_1, request >( const request& other );
    template<> request from_string_impl< version::http_1_1, request >( const std::string& str );

    // response
    template<> std::string to_string_impl< version::http_1_1, response >( const response& other );
    template<> response from_string_impl< version::http_1_1, response >( const std::string& str );

  } // detail

} } // fc::http