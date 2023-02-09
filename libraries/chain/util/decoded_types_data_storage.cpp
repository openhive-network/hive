#include <hive/chain/util/decoded_types_data_storage.hpp>



namespace hive { namespace chain { namespace util {

decoded_type_data::decoded_type_data(const fc::ripemd160& _checksum, const std::string_view _name, members_vector&& _members, enum_values_vector _enum_values)
  : members(std::move(_members)), enum_values(_enum_values), checksum(_checksum), name(_name)
{
  if (name.empty())
    FC_THROW_EXCEPTION( fc::invalid_arg_exception, "Decoded type - name cannot be empty" );
  
  if (members.empty() && enum_values.empty())
    FC_THROW_EXCEPTION( fc::invalid_arg_exception, "Members or enum_values vector have to be specified." );
  else if (!members.empty() && !enum_values.empty())
    FC_THROW_EXCEPTION( fc::invalid_arg_exception, "Members and enum_values vector cannot be specified together." );

  dlog("New decoded type: ${name}, checksum: ${checksum}", ("name", std::string(name))("checksum", checksum));
}

decoded_types_data_storage::decoded_types_data_storage()
{
  dlog("decoded_types_data_storage object has been created.");
}

decoded_types_data_storage::~decoded_types_data_storage()
{
  dlog("decoded_types_data_storage object has been deleted");
}

decoded_types_data_storage& decoded_types_data_storage::get_instance()
{
  static decoded_types_data_storage instance;
  return instance;
}

void decoded_types_data_storage::add_type_to_decoded_types_set(const std::string_view type_name)
{
  decoded_types_set.emplace(type_name);
}

void decoded_types_data_storage::add_decoded_type_data_to_map(decoded_type_data&& decoded_type)
{
  decoded_types_data_map.try_emplace(decoded_type.get_type_name(), std::move(decoded_type));
}

bool decoded_types_data_storage::type_already_decoded(const std::string_view type_name)
{
  if (decoded_types_set.find(type_name) != decoded_types_set.end())
    return true;

  else
    return false;
}

} } } // hive::chain::util