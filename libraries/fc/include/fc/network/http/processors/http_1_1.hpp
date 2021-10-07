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
  } // detail

} } // fc::http