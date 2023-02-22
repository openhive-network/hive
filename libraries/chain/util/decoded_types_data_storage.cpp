#include <hive/chain/util/decoded_types_data_storage.hpp>



namespace hive { namespace chain { namespace util {

fc::ripemd160 calculate_checksum_from_string(const std::string_view str)
{
  fc::ripemd160::encoder encoder;
  encoder.write(str.data(), str.size());
  return encoder.result();
}

decoded_type_data::decoded_type_data(const fc::ripemd160& _checksum, const std::string_view _type_id, const bool _reflected)
  : checksum(_checksum), type_id(_type_id), reflected(_reflected)
{
  if (type_id.empty())
    FC_THROW_EXCEPTION( fc::invalid_arg_exception, "Decoded type - name cannot be empty" );

  if (!reflected)
    dlog("New non reflected decoded type. Typeid: ${type_id}, checksum: ${checksum}", (type_id)(checksum));
}

reflected_decoded_type_data::reflected_decoded_type_data(const fc::ripemd160& _checksum, const std::string_view _type_id, const std::string_view _fc_name,
                                                         members_vector&& _members, enum_values_vector&& _enum_values)
  : decoded_type_data(_checksum, _type_id, true), members(std::move(_members)), enum_values(std::move(_enum_values)), fc_name(_fc_name)
{
  if (fc_name.empty())
    FC_THROW_EXCEPTION( fc::invalid_arg_exception, "Decoded type - name cannot be empty" );
  
  if (members.empty() && enum_values.empty())
    FC_THROW_EXCEPTION( fc::invalid_arg_exception, "Members or enum_values vector have to be specified." );
  else if (!members.empty() && !enum_values.empty())
    FC_THROW_EXCEPTION( fc::invalid_arg_exception, "Members and enum_values vector cannot be specified together." );

  dlog("New reflected decoded type: ${fc_name}, checksum: ${checksum}", (fc_name)(checksum));
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

bool decoded_types_data_storage::add_type_to_decoded_types_set(const std::string_view type_name)
{
  return decoded_types_set.emplace(type_name).second;
}

void decoded_types_data_storage::add_decoded_type_data_to_map(const std::shared_ptr<decoded_type_data>& decoded_type)
{
  decoded_types_data_map.try_emplace(decoded_type->get_type_id(), decoded_type);
}

} } } // hive::chain::util