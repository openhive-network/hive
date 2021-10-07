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
  } // detail

} } // fc::http