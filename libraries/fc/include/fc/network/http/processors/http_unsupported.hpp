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

      virtual std::string parse_request( const request& r )const override;
      virtual request     parse_request( const std::string& r )const override;

      virtual std::string parse_response( const response& r )const override;
      virtual response    parse_response( const std::string& r )const override;
    };

  } // detail

} } // fc::http