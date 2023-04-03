#include <hive/chain/util/type_registrar.hpp>
#include <hive/chain/util/decoded_types_data_storage.hpp>

#define HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(type_name) \
  template <> \
  void hive::chain::util::type_registrar<type_name>::register_type(hive::chain::util::decoded_types_data_storage& dtds) \
  { \
    dtds.register_new_type<type_name>();  \
  }
