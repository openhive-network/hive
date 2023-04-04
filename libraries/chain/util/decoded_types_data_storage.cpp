#include <hive/chain/util/decoded_types_data_storage.hpp>

#include <hive/protocol/types.hpp>

namespace hive { namespace chain { namespace util {

const std::string HIVE_VOID_T = boost::core::demangle(typeid(hive::void_t).name());

std::string calculate_checksum_from_string(const std::string_view str)
{
  fc::ripemd160::encoder encoder;
  encoder.write(str.data(), str.size());
  return std::move(encoder.result().str());
}

decoded_type_data::decoded_type_data(const std::string_view _checksum, const std::string_view _type_name, const size_t _type_size, const size_t type_align)
  : size_of(_type_size), align_of(type_align), name(_type_name), checksum(_checksum)
{
  if (name.empty())
    FC_THROW_EXCEPTION( fc::invalid_arg_exception, "Decoded type - type_name cannot be empty");
  if (checksum.empty())
    FC_THROW_EXCEPTION( fc::invalid_arg_exception, "Decoded type - checksum cannot be empty");
  if (!align_of)
    FC_THROW_EXCEPTION( fc::invalid_arg_exception, "Decoded type - align_of must be set");
  if (!size_of)
    FC_THROW_EXCEPTION( fc::invalid_arg_exception, "Decoded type - size_of must be set");

  reflected = false;
}

decoded_type_data::decoded_type_data(const std::string_view _checksum, const std::string_view _type_name, const size_t _type_size, const size_t type_align,
                    members_vector_t&& _members)
  : members(std::move(_members)), size_of(_type_size), align_of(type_align), name(_type_name), checksum(_checksum)
{

  if (checksum.empty())
    FC_THROW_EXCEPTION( fc::invalid_arg_exception, "Decoded type - checksum cannot be empty");
  if (name.empty())
    FC_THROW_EXCEPTION( fc::invalid_arg_exception, "Decoded type - field name must be specified." );
  //at the moment only hive::void_t is reflected structure with no members.
  if (members->empty() && name != HIVE_VOID_T)
    FC_THROW_EXCEPTION( fc::invalid_arg_exception, "Members have to be specified. Type name: ${name}", (name) );
  if (!align_of)
    FC_THROW_EXCEPTION( fc::invalid_arg_exception, "Decoded type - align_of must be set");
  if (!size_of)
    FC_THROW_EXCEPTION( fc::invalid_arg_exception, "Decoded type - size_of must be set");

  reflected = true;
}

decoded_type_data::decoded_type_data(const std::string_view _checksum, const std::string_view _type_name, enum_values_vector_t&& _enum_values)
  : enum_values(std::move(_enum_values)), name(_type_name), checksum(_checksum)
{
  if (checksum.empty())
    FC_THROW_EXCEPTION( fc::invalid_arg_exception, "Decoded type - checksum cannot be empty");
  if (name.empty())
    FC_THROW_EXCEPTION( fc::invalid_arg_exception, "Decoded type - field name must be specified." );
  if (enum_values->empty())
    FC_THROW_EXCEPTION( fc::invalid_arg_exception, "Enum values have to be specified. Type name: ${name}", (name) );

  reflected = true;
}

decoded_type_data::decoded_type_data(const std::string& json)
{
  if (json.empty())
    FC_THROW_EXCEPTION( fc::invalid_arg_exception, "Json with decoded types data cannot be empty.");

  try
  {
  fc::from_variant(fc::json::from_string(json), *this);
  }
  FC_RETHROW_EXCEPTIONS(error, "Cannot create decoded_type_data from json: ${json}", (json))

  if (checksum.empty() || name.empty())
    FC_THROW_EXCEPTION( fc::invalid_arg_exception, "Json misses basic decoded type data. ${json}", (json));

  //at the moment only hive::void_t is reflected structure with no members.
  if (reflected && (((!members || members->empty()) && (!enum_values || enum_values->empty()) && name != HIVE_VOID_T)))
    FC_THROW_EXCEPTION( fc::invalid_arg_exception, "Json with reflected decoded type doesn't contain enough data. ${json}", (json));
  else if (!reflected && (members || enum_values))
    FC_THROW_EXCEPTION( fc::invalid_arg_exception, "Json with non reflected decoded type contains data for reflected type. ${json}", (json));
  
  if (enum_values->empty() && (!size_of || !align_of))
    FC_THROW_EXCEPTION( fc::invalid_arg_exception, "Json with decoded type should contains data about sizeof and alignof if type is not an reflected enum. ${json}", (json));
  else if (!enum_values->empty() && (size_of || align_of))
    FC_THROW_EXCEPTION( fc::invalid_arg_exception, "Json with decoded reflected type should not contains data about sizeof and alignof. ${json}", (json));
}

decoded_types_data_storage::~decoded_types_data_storage()
{
  dlog("decoded_types_data_storage object has been deleted. Decoded types map size: ${map_size}", ("map_size", decoded_types_data_map.size()));
}

bool decoded_types_data_storage::add_type_to_decoded_types_set(const std::string_view type_id)
{
  return decoded_types_set.emplace(type_id).second;
}

void decoded_types_data_storage::add_decoded_type_data_to_map(decoded_type_data&& decoded_type)
{
  decoded_types_data_map.try_emplace(decoded_type.name, std::move(decoded_type));
}

std::string decoded_types_data_storage::generate_decoded_types_data_json_string() const
{
  std::vector<decoded_type_data> decoded_types_vector;
  decoded_types_vector.reserve(decoded_types_data_map.size());

  for (const auto& [key, decoded_type] : decoded_types_data_map)
    decoded_types_vector.push_back(decoded_type);

  return fc::json::to_string(decoded_types_vector);
}

decoded_types_data_storage::decoded_types_map_t generate_data_map_from_json(const std::string& decoded_types_data_json)
{
  if (decoded_types_data_json.empty())
    FC_THROW_EXCEPTION( fc::invalid_arg_exception, "Json with decoded types data cannot be empty.");

  try
  {
    fc::json::is_valid(decoded_types_data_json);
  }
  FC_RETHROW_EXCEPTIONS(error, "Json is invalid: ${decoded_types_data_json}", (decoded_types_data_json))

  const auto json = fc::json::from_string(decoded_types_data_json);

  if (!json.is_array())
    FC_THROW_EXCEPTION( fc::invalid_arg_exception, "Json with decoded types data is not valid. Expecting a an array of objects. ${decoded_types_data_json}", (decoded_types_data_json));

  const auto& json_array = json.get_array();
  decoded_types_data_storage::decoded_types_map_t decoded_types_map;

  for (const fc::variant& type_data : json_array)
  {
    if (!type_data.is_object())
      FC_THROW_EXCEPTION( fc::invalid_arg_exception, "Json with decoded types data is not valid. Expecting a json object with decoded type data. ${decoded_types_data_json}", (decoded_types_data_json));

    const decoded_type_data loaded_decoded_type(fc::json::to_string(type_data));
    decoded_types_map.try_emplace(loaded_decoded_type.name, std::move(loaded_decoded_type));
  }

  return decoded_types_map;
}

std::string decoded_types_data_storage::generate_decoded_types_data_pretty_string(const std::string& decoded_types_data_json) const
{
  auto generate_pretty_string_from_map = [](const auto& decoded_types_map) -> std::string
  {
    std::stringstream ss;
    ss << "\nNumber of objects in map: " << decoded_types_map.size() << "\n\n";

    for (const auto& [key, decoded_type] : decoded_types_map)
      ss << fc::json::to_pretty_string(decoded_type) << "\n\n";

    return ss.str();
  };

  if (decoded_types_data_json.empty())
    return generate_pretty_string_from_map(decoded_types_data_map);
  else
    return generate_pretty_string_from_map(generate_data_map_from_json(decoded_types_data_json));
}

std::pair<bool, std::string> decoded_types_data_storage::check_if_decoded_types_data_json_matches_with_current_decoded_data(const std::string& decoded_types_data_json) const
{
  auto loaded_decoded_types_map = generate_data_map_from_json(decoded_types_data_json);
  bool no_difference_detected = true;
  std::stringstream error_ss;

  /* Check if both maps have the same size. */
  const size_t loaded_decoded_types_data_map_size = loaded_decoded_types_map.size();
  const size_t current_decoded_types_data_map_size = decoded_types_data_map.size();

  if (loaded_decoded_types_data_map_size != current_decoded_types_data_map_size)
  {
    no_difference_detected = false;
    error_ss << "Amount of decoded types differs from amount of loaded decoded types. Current amount of decoded types: " << std::to_string(current_decoded_types_data_map_size)
             << ", loaded amount of decoded types: " << std::to_string(loaded_decoded_types_data_map_size) << "\n";
  }

  for (const auto& [dtdm_key, dtdm_decoded_type] : decoded_types_data_map)
  {
    if (loaded_decoded_types_map.find(dtdm_key) == loaded_decoded_types_map.end())
    {
      if (no_difference_detected)
        no_difference_detected = false;

      error_ss << "Type is in current decoded types map but not in loaded decoded types map: " << dtdm_decoded_type.name << "\n";
    }
    else
    {
      if (const auto& ldtm_decoded_type = loaded_decoded_types_map.at(dtdm_key);
          dtdm_decoded_type.checksum != ldtm_decoded_type.checksum)
      {
        if (no_difference_detected)
          no_difference_detected = false;

        error_ss << "Reflected type: " << dtdm_decoded_type.name
                 << " has checksum: " << dtdm_decoded_type.checksum
                 << ", which diffs from loaded type: " << ldtm_decoded_type.checksum << "\n";
      }
    }

    loaded_decoded_types_map.erase(dtdm_key);
  }

  for (const auto& [ldtm_key, ldtm_decoded_type] : loaded_decoded_types_map)
  {
    if (no_difference_detected)
      no_difference_detected = false;

    error_ss << "Type is in loaded decoded types map but not in current decoded types map: " << ldtm_decoded_type.name << "\n";
  }

  return std::pair<bool, std::string>(no_difference_detected, error_ss.str());
}

} } } // hive::chain::util