#include <hive/chain/util/operation_extractor.hpp>
#include <hive/chain/util/type_extractor_processor.hpp>

#include <fc/reflect/typename.hpp>

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
      auto _name        = boost::typeindex::type_id<T>().pretty_name();
      auto _short_name  = fc::trim_typename_namespace( _name );
      int64_t _idx      = static_cast<int64_t>( operations_details.size() );

      operations_ids[ _short_name ] = _idx;
      operations_details[ _idx ]    = std::pair<string, bool>( _name, T{}.is_virtual() );
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
