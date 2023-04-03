#pragma once

namespace hive { namespace chain { namespace util {

class decoded_types_data_storage;

class abstract_type_registrar
{
  public:
    virtual void register_type(decoded_types_data_storage& dtds) = 0;
};

template <typename T>
class type_registrar final : public abstract_type_registrar
{
  void register_type(decoded_types_data_storage& dtds) override;
};

} } } // hive::chain::util