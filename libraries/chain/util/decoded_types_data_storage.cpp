#include <hive/chain/util/decoded_types_data_storage.hpp>



namespace hive { namespace chain { namespace util {

decoded_type_data::decoded_type_data(const size_t _checksum, const std::string_view _name, members_vector&& _members, enum_values_vector _enum_values)
  : checksum(_checksum), name(_name), members(std::move(_members)), enum_values(_enum_values)
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

void decoded_types_data_storage::add_type_to_decoding_set(const std::string_view type_name)
{
  std::lock_guard<std::shared_mutex> write_lock(mutex);
  types_being_decoded_set.emplace(type_name);
}

void decoded_types_data_storage::remove_type_from_decoding_set(const std::string_view type_name)
{
  std::lock_guard<std::shared_mutex> write_lock(mutex);
  types_being_decoded_set.erase(type_name);
}

void decoded_types_data_storage::add_decoded_type_data_to_map(decoded_type_data&& decoded_type)
{
  std::lock_guard<std::shared_mutex> write_lock(mutex);
  decoded_types_data_map.try_emplace(decoded_type.get_type_name(), std::move(decoded_type));
}

bool decoded_types_data_storage::type_is_being_decoded_or_already_decoded(const std::string_view type_name)
{
  std::shared_lock<std::shared_mutex> read_lock(mutex);
  if (types_being_decoded_set.find(type_name) != types_being_decoded_set.end() ||
      decoded_types_data_map.find(type_name) != decoded_types_data_map.end())
    return true;

  else
    return false;
}

} } } // hive::chain::util