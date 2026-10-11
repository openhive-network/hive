#include <hive/chain/util/decoded_types_data_storage.hpp>

#include <hive/protocol/types.hpp>

#include <algorithm>

namespace hive { namespace chain { namespace util {

const std::string HIVE_VOID_T = boost::core::demangle(typeid(hive::void_t).name());

std::string calculate_checksum_from_string(const std::string_view str)
{
  fc::ripemd160::encoder encoder;
  encoder.write(str.data(), str.size());
  return encoder.result().str();
}

namespace canonical_names
{

std::string normalize_type_name(std::string type_name)
{
  for (const std::string_view implementation_namespace : {"std::__cxx11::", "std::__1::"})
  {
    for (size_t pos = type_name.find(implementation_namespace); pos != std::string::npos; pos = type_name.find(implementation_namespace, pos))
      type_name.replace(pos, implementation_namespace.size(), "std::");
  }

  return type_name;
}

std::string template_name(const std::string& demangled_type_name)
{
  if (demangled_type_name.empty() || demangled_type_name.back() != '>')
    return normalize_type_name(demangled_type_name);

  size_t depth = 0;
  for (size_t pos = demangled_type_name.size(); pos-- > 0;)
  {
    if (demangled_type_name[pos] == '>')
      ++depth;
    else if (demangled_type_name[pos] == '<' && --depth == 0)
      return normalize_type_name(demangled_type_name.substr(0, pos));
  }

  FC_THROW_EXCEPTION( fc::invalid_arg_exception, "Unbalanced template argument list in type name: ${demangled_type_name}", (demangled_type_name) );
}

std::string non_template_type_name(const std::string& demangled_type_name)
{
  if (demangled_type_name.find('<') != std::string::npos)
    FC_THROW_EXCEPTION( fc::invalid_arg_exception, "Template arguments of ${demangled_type_name} have to be named canonically - by canonical_names::value_template_arguments", (demangled_type_name) );

  return normalize_type_name(demangled_type_name);
}

std::string join_template_arguments(const std::string& template_name, const std::vector<std::string>& arguments)
{
  std::string result = template_name + "<";

  for (size_t i = 0; i < arguments.size(); ++i)
  {
    if (i)
      result += ", ";
    result += arguments[i];
  }

  return result + ">";
}

} // canonical_names

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
  if ((!members || members->empty()) && name != HIVE_VOID_T)
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
  if (!enum_values || enum_values->empty())
    FC_THROW_EXCEPTION( fc::invalid_arg_exception, "Enum values have to be specified. Type name: ${name}", (name) );

  reflected = true;
}

decoded_type_data::decoded_type_data(const std::string& json)
{
  if (json.empty())
    FC_THROW_EXCEPTION( fc::invalid_arg_exception, "Json with decoded types data cannot be empty.");

  try
  {
    fc::variant v = fc::json::from_string(json, fc::json::format_validation_mode::full);
    fc::variant_object vo = v.get_object();
    if (!vo.contains("reflected"))
      FC_THROW_EXCEPTION( fc::invalid_arg_exception, "Json with decoded type data misses reflected flag. ${json}", (json));

    fc::from_variant(v, *this);
  }
  FC_RETHROW_EXCEPTIONS(error, "Cannot create decoded_type_data from json: ${json}", (json))

  if (checksum.empty() || name.empty())
    FC_THROW_EXCEPTION( fc::invalid_arg_exception, "Json misses basic decoded type data. ${json}", (json));

  //at the moment only hive::void_t is reflected structure with no members.
  if (reflected && (((!members || members->empty()) && (!enum_values || enum_values->empty()) && name != HIVE_VOID_T)))
    FC_THROW_EXCEPTION( fc::invalid_arg_exception, "Json with reflected decoded type doesn't contain enough data. ${json}", (json));
  else if (!reflected && (members || enum_values))
    FC_THROW_EXCEPTION( fc::invalid_arg_exception, "Json with non reflected decoded type contains data for reflected type. ${json}", (json));
  
  if ((!enum_values || enum_values->empty()) && (!size_of || !align_of))
    FC_THROW_EXCEPTION( fc::invalid_arg_exception, "Json with decoded type should contains data about sizeof and alignof if type is not an reflected enum. ${json}", (json));
  else if ((enum_values && !enum_values->empty()) && (size_of || align_of))
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

void decoded_types_data_storage::add_serialized_type_data_to_map(serialized_type_data&& serialized_type)
{
  serialized_types_data_map.try_emplace(serialized_type.name, std::move(serialized_type));
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
    fc::json::is_valid(decoded_types_data_json, fc::json::format_validation_mode::full);
  }
  FC_RETHROW_EXCEPTIONS(error, "Json is invalid: ${decoded_types_data_json}", (decoded_types_data_json))

  const auto json = fc::json::from_string(decoded_types_data_json, fc::json::format_validation_mode::full);

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

namespace
{

template <typename TypesMap>
std::string generate_pretty_string_from_map(const TypesMap& types_map)
{
  std::stringstream ss;
  ss << "\nNumber of objects in map: " << types_map.size() << "\n\n";

  for (const auto& [key, type_data] : types_map)
    ss << fc::json::to_pretty_string(type_data) << "\n\n";

  return ss.str();
}

} // namespace

std::string decoded_types_data_storage::generate_decoded_types_data_pretty_string(const std::string& decoded_types_data_json) const
{
  if (decoded_types_data_json.empty())
    return generate_pretty_string_from_map(decoded_types_data_map);
  else
    return generate_pretty_string_from_map(generate_data_map_from_json(decoded_types_data_json));
}

std::pair<state_definitions_verification_result, std::string> decoded_types_data_storage::check_if_decoded_types_data_json_matches_with_current_decoded_data(const std::string& decoded_types_data_json) const
{
  auto loaded_decoded_types_map = generate_data_map_from_json(decoded_types_data_json);
  state_definitions_verification_result comparision_result = state_definitions_verification_result::state_definitions_matches;
  std::stringstream error_ss;

  /* Check if both maps have the same size. */
  const size_t loaded_decoded_types_data_map_size = loaded_decoded_types_map.size();
  const size_t current_decoded_types_data_map_size = decoded_types_data_map.size();

  if (loaded_decoded_types_data_map_size != current_decoded_types_data_map_size)
    error_ss << "Amount of decoded types differs from amount of loaded decoded types. Current amount of decoded types: " << std::to_string(current_decoded_types_data_map_size)
             << ", loaded amount of decoded types: " << std::to_string(loaded_decoded_types_data_map_size) << "\n";

  for (const auto& [dtdm_key, dtdm_decoded_type] : decoded_types_data_map)
  {
    if (loaded_decoded_types_map.find(dtdm_key) == loaded_decoded_types_map.end())
    {
      // Throw an exception - there are more definitions in current hived than shm/snapshot than.
      if (comparision_result != state_definitions_verification_result::mismatch_throw_error)
        comparision_result = state_definitions_verification_result::mismatch_throw_error;

      error_ss << "Type is in current decoded types map but not in loaded decoded types map: " << dtdm_decoded_type.name << "\n";
    }
    else
    {
      if (const auto& ldtm_decoded_type = loaded_decoded_types_map.at(dtdm_key);
          dtdm_decoded_type.checksum != ldtm_decoded_type.checksum)
      {
        if (comparision_result != state_definitions_verification_result::mismatch_throw_error)
          comparision_result = state_definitions_verification_result::mismatch_throw_error;

        error_ss << "Reflected type: " << dtdm_decoded_type.name
                 << " has checksum: " << dtdm_decoded_type.checksum
                 << ", which diffs from loaded type: " << ldtm_decoded_type.checksum << "\n";
      }
    }

    loaded_decoded_types_map.erase(dtdm_key);
  }

  for (const auto& [ldtm_key, ldtm_decoded_type] : loaded_decoded_types_map)
  {
    // Log warning - there are more definitions in shm/snapshot than in current hived.
    if (comparision_result != state_definitions_verification_result::mismatch_throw_error && comparision_result != state_definitions_verification_result::mismatch_log_warning)
      comparision_result = state_definitions_verification_result::mismatch_log_warning;

    error_ss << "Type is in loaded decoded types map but not in current decoded types map: " << ldtm_decoded_type.name << "\n";
  }

  return std::pair<state_definitions_verification_result, std::string>(comparision_result, error_ss.str());
}

std::string decoded_types_data_storage::generate_serialized_types_data_json_string() const
{
  std::vector<serialized_type_data> serialized_types_vector;
  serialized_types_vector.reserve(serialized_types_data_map.size());

  for (const auto& [key, serialized_type] : serialized_types_data_map)
    serialized_types_vector.push_back(serialized_type);

  return fc::json::to_string(serialized_types_vector);
}

namespace
{

decoded_types_data_storage::serialized_types_map_t generate_serialized_data_map_from_json(const std::string& serialized_types_data_json)
{
  if (serialized_types_data_json.empty())
    FC_THROW_EXCEPTION( fc::invalid_arg_exception, "Json with serialized types data cannot be empty.");

  std::vector<serialized_type_data> serialized_types_vector;

  try
  {
    fc::from_variant(fc::json::from_string(serialized_types_data_json, fc::json::format_validation_mode::full), serialized_types_vector);
  }
  FC_RETHROW_EXCEPTIONS(error, "Json with serialized types data is invalid: ${serialized_types_data_json}", (serialized_types_data_json))

  decoded_types_data_storage::serialized_types_map_t serialized_types_map;

  for (serialized_type_data& serialized_type : serialized_types_vector)
  {
    if (serialized_type.name.empty() || serialized_type.kind.empty())
      FC_THROW_EXCEPTION( fc::invalid_arg_exception, "Json with serialized types data misses name or kind of a type. ${serialized_types_data_json}", (serialized_types_data_json));

    std::string name = serialized_type.name;
    serialized_types_map.try_emplace(std::move(name), std::move(serialized_type));
  }

  return serialized_types_map;
}

std::string describe_entry(const serialized_type_data::entries_vector_t& entries, const size_t index)
{
  if (index >= entries.size())
    return "nothing";

  return "`" + entries[index].name + ": " + entries[index].value + "'";
}

/* Describes the first difference between two descriptors of the same type. */
std::string describe_first_difference(const serialized_type_data& current, const serialized_type_data& loaded)
{
  std::stringstream ss;
  ss << "Type: " << current.name << " is serialized differently";

  if (current.kind != loaded.kind)
  {
    ss << " - it is a " << current.kind << " in current version, but a " << loaded.kind << " in loaded data.";
    return ss.str();
  }

  for (size_t i = 0; i < std::max(current.entries.size(), loaded.entries.size()); ++i)
  {
    if (i >= current.entries.size() || i >= loaded.entries.size() || !(current.entries[i] == loaded.entries[i]))
    {
      ss << " - " << current.kind << " entry #" << i << " is " << describe_entry(current.entries, i)
         << " in current version, but " << describe_entry(loaded.entries, i) << " in loaded data.";
      return ss.str();
    }
  }

  return ss.str();
}

} // namespace

std::string decoded_types_data_storage::generate_serialized_types_data_pretty_string(const std::string& serialized_types_data_json) const
{
  if (serialized_types_data_json.empty())
    return generate_pretty_string_from_map(serialized_types_data_map);
  else
    return generate_pretty_string_from_map(generate_serialized_data_map_from_json(serialized_types_data_json));
}

std::pair<state_definitions_verification_result, std::string> decoded_types_data_storage::check_if_serialized_types_data_json_matches_with_current_serialized_data(const std::string& serialized_types_data_json) const
{
  auto loaded_serialized_types_map = generate_serialized_data_map_from_json(serialized_types_data_json);
  state_definitions_verification_result comparison_result = state_definitions_verification_result::state_definitions_matches;
  std::stringstream error_ss;

  for (const auto& [name, current_type] : serialized_types_data_map)
  {
    const auto loaded_it = loaded_serialized_types_map.find(name);

    if (loaded_it == loaded_serialized_types_map.end())
    {
      comparison_result = state_definitions_verification_result::mismatch_throw_error;
      error_ss << "Type is in current serialized types map but not in loaded serialized types map: " << name << "\n";
      continue;
    }

    if (!(current_type == loaded_it->second))
    {
      comparison_result = state_definitions_verification_result::mismatch_throw_error;
      error_ss << describe_first_difference(current_type, loaded_it->second) << "\n";
    }

    loaded_serialized_types_map.erase(loaded_it);
  }

  for (const auto& [name, loaded_type] : loaded_serialized_types_map)
  {
    // Log warning - there are more definitions in loaded data than in current hived.
    if (comparison_result == state_definitions_verification_result::state_definitions_matches)
      comparison_result = state_definitions_verification_result::mismatch_log_warning;

    error_ss << "Type is in loaded serialized types map but not in current serialized types map: " << name << "\n";
  }

  return std::pair<state_definitions_verification_result, std::string>(comparison_result, error_ss.str());
}

decoded_types_data_storage::decoded_types_data_storage(const std::string& decoded_types_data_json)
{
  decoded_types_data_map = generate_data_map_from_json(decoded_types_data_json);
}

} } } // hive::chain::util
