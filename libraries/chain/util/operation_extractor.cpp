#include <hive/chain/util/operation_extractor.hpp>
#include <hive/chain/util/type_extractor_processor.hpp>

#include <boost/type_index.hpp>

namespace type_extractor
{
  struct typename_gatherer
  {
    operation_extractor::operation_ids_container_t&     operations_ids;
    operation_extractor::operation_details_container_t& operations_details;

    template <typename T, typename SV>
    void operator()(boost::type<boost::tuple<T, SV>> t) const
    {
      //TODO: process of `operations_ids`
      operations_details[ operations_details.size() ] = std::pair<string, bool>(boost::typeindex::type_id<T>().pretty_name(), T{}.is_virtual());
    }
  };

  operation_extractor::operation_extractor()
  {
    typedef sv_processor<hive::protocol::operation> processor;

    typename_gatherer p{ operations_ids, operations_details };
    boost::mpl::for_each<processor::transformed_type_list, boost::type<boost::mpl::_>>(p);
  }

  const operation_extractor::operation_ids_container_t& operation_extractor::get_operation_ids() const
  {
    return operations_ids;
  }

  const operation_extractor::operation_details_container_t& operation_extractor::get_operation_details() const
  {
    return operations_details;
  }

} // namespace type_extractor
