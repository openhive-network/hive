#include <hive/chain/util/type_registrar.hpp>
#include <hive/chain/util/decoded_types_data_storage.hpp>

template <class T>
void hive::chain::util::type_registrar<T>::register_type(hive::chain::util::decoded_types_data_storage& dtds)
{
dtds.register_new_type<T>();
}

#define HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(type_name) \
  template class hive::chain::util::type_registrar<type_name>;
