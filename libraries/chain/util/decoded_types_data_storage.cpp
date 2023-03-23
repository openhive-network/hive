#include <hive/chain/util/decoded_types_data_storage.hpp>

namespace hive { namespace chain { namespace util {

std::string calculate_checksum_from_string(const std::string_view str)
{
  fc::ripemd160::encoder encoder;
  encoder.write(str.data(), str.size());
  return std::move(encoder.result().str());
}

decoded_type_data::decoded_type_data(const std::string_view _checksum, const std::string_view _type_id, const bool _reflected)
  : checksum(_checksum), type_id(_type_id), reflected(_reflected)
{
  if (type_id.empty())
    FC_THROW_EXCEPTION( fc::invalid_arg_exception, "Decoded type - name cannot be empty" );
}

decoded_type_data::decoded_type_data(const std::vector<fc::variant>& json)
{
  const size_t json_size = json.size();
  if (json_size < 3)
    FC_THROW_EXCEPTION( fc::invalid_arg_exception, "Cannot create decoded_type_data object because expect 3 elements in array json. ${json}", (json));

  try
  {
    auto type_is_reflected = json[0].as_int64();
    if (type_is_reflected != 0 && type_is_reflected != 1)
      FC_THROW_EXCEPTION(fc::invalid_arg_exception, "Expecting 0 or 1 on 1th position of json. (1st element shows if type is reflected)");

    reflected = bool(type_is_reflected);
    type_id = json[1].as_string();
    checksum = json[2].as_string();
  }
  catch (fc::exception& e)
  {
    FC_THROW_EXCEPTION(fc::invalid_arg_exception, "Cannot create decode type data from json. What: ${problem}, json: ${json}", ("problem", e.to_detail_string())(json));
  }
  catch(...)
  {
    FC_THROW("Unspecified error occured during creation of decoded_type_data from json ${json}", (json));
  }

  if (reflected && json_size != 6)
    FC_THROW_EXCEPTION( fc::invalid_arg_exception, "Cannot create decoded_type_data object because in case of reflected type expecting 6 elements in array json. ${json}", (json));
  else if (!reflected && json_size != 3)
    FC_THROW_EXCEPTION( fc::invalid_arg_exception, "Cannot create decoded_type_data object because in case of non reflected type expecting 3 elements in array json. ${json}", (json));
}

std::string decoded_type_data::to_json_string() const
{
  std::stringstream ss;
  ss << "[" << reflected << ",\"" << type_id << "\",\"" << checksum << "\"]";
  const std::string result = ss.str();

  if (!fc::json::is_valid(result))
    FC_THROW_EXCEPTION( fc::parse_error_exception, "Created json for type: ${type_id} is not valid: %{result}", (type_id)(result) );

  return result;
}

reflected_decoded_type_data::reflected_decoded_type_data(const std::string_view _checksum, const std::string_view _type_id, const std::string_view _fc_name,
                                                         members_vector&& _members, enum_values_vector&& _enum_values)
  : decoded_type_data(_checksum, _type_id, true), members(std::move(_members)), enum_values(std::move(_enum_values)), fc_name(_fc_name)
{
  if (fc_name.empty())
    FC_THROW_EXCEPTION( fc::invalid_arg_exception, "Decoded type - name cannot be empty" );
  
  if (members.empty() && enum_values.empty())
    FC_THROW_EXCEPTION( fc::invalid_arg_exception, "Members or enum_values vector have to be specified." );
  else if (!members.empty() && !enum_values.empty())
    FC_THROW_EXCEPTION( fc::invalid_arg_exception, "Members and enum_values vector cannot be specified together." );
}

reflected_decoded_type_data::reflected_decoded_type_data(const std::vector<fc::variant>& json) : decoded_type_data(json)
{
  try
  {
    fc_name = json[3].as_string();
    auto type_is_enum = json[4].as_int64();

    if (type_is_enum != 0 && type_is_enum != 1)
      FC_THROW_EXCEPTION(fc::invalid_arg_exception, "Expecting 0 or 1 on 5th position of json. (5th element shows if type is an enum)");

    const bool is_enum = bool(type_is_enum);
    const fc::variant_object& variant_obj = json[5].get_object();

    if (is_enum)
    {
      enum_values.reserve(variant_obj.size());

      for(const auto& el : variant_obj)
        enum_values.push_back({el.key(), el.value().as_uint64()});

      enum_values.shrink_to_fit();
    }
    else
    {
      members.reserve(variant_obj.size());

      for(const auto& el : variant_obj)
        members.push_back({el.key(), el.value().as_string()});

      members.shrink_to_fit();
    }
  }
  catch (fc::exception& e)
  {
    FC_THROW_EXCEPTION(fc::invalid_arg_exception, "Cannot create decode type data from json. What: ${problem}, json: ${json}", ("problem", e.to_detail_string())(json));
  }
  catch(...)
  {
    FC_THROW("Unspecified error occured during creation of reflected decoded_type_data from json ${json}", (json));
  }
}

std::string reflected_decoded_type_data::to_json_string() const
{
  const bool type_is_enum = is_enum();
  std::stringstream ss;
  ss << "[" << reflected << ",\"" << type_id << "\",\"" << checksum << "\",\"" << fc_name << "\"," << type_is_enum << ",{";
  if (type_is_enum)
  {
    for (const auto& [name, value] : enum_values)
      ss << "\"" << name << "\":" << value << ",";
    ss.seekp(-1, std::ios_base::end); // replace , with }
    ss << "}";
  }
  else
  {
    for (const auto& [type, name] : members)
      ss << "\"" << type << "\":\"" << name << "\",";
    ss.seekp(-1, std::ios_base::end); // replace , with }
    ss << "}";
  }

  ss << "]";

  const std::string result = ss.str();

  if (!fc::json::is_valid(result))
    FC_THROW_EXCEPTION( fc::parse_error_exception, "Created json for type: ${fc_name} is not valid: %{result}", (fc_name)(result) );

  return result;
}

decoded_types_data_storage::decoded_types_data_storage()
{
  dlog("decoded_types_data_storage object has been created.");
}

decoded_types_data_storage::~decoded_types_data_storage()
{
  dlog("decoded_types_data_storage object has been deleted. Decoded types map size: ${map_size}", ("map_size", decoded_types_data_map.size()));
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

std::string decoded_types_data_storage::generate_decoded_types_data_json_string() const
{
  std::stringstream ss;
  ss << "[";

  for (const auto& item : decoded_types_data_map)
    ss << item.second->to_json_string() << ",";

  ss.seekp(-1, std::ios_base::end); // replace , with }
  ss << "]";

  const std::string result = ss.str();

  if (!fc::json::is_valid(result))
    FC_THROW_EXCEPTION( fc::parse_error_exception, "Created json for whole map is not valid: %{result}", (result) );

  return result;
}

std::unordered_map<std::string_view, std::unique_ptr<decoded_type_data>> generate_data_map_from_json(const std::string& decoded_types_data_json)
{
  if (decoded_types_data_json.empty())
    FC_THROW_EXCEPTION( fc::invalid_arg_exception, "Json with decoded types data cannot be empty.");

  try
  {
    fc::json::is_valid(decoded_types_data_json);
  }
  catch (fc::parse_error_exception& e)
  {
    FC_THROW_EXCEPTION( fc::invalid_arg_exception, "Json with decoded types data is not valid, cannot parse json. Error: ${error}, Json: ${decoded_types_data_json}", ("error", e.to_detail_string())(decoded_types_data_json));
  }
  catch(...)
  {
    FC_THROW_EXCEPTION( fc::invalid_arg_exception, "Unspecified error occurred during parsing json with decoded types data. ${decoded_types_data_json}", (decoded_types_data_json));
  }

  const auto json = fc::json::from_string(decoded_types_data_json);

  if (!json.is_array())
    FC_THROW_EXCEPTION( fc::invalid_arg_exception, "Json with decoded types data is not valid. Expecting a json array. ${decoded_types_data_json}", (decoded_types_data_json));

  const auto& json_array = json.get_array();

  std::unordered_map<std::string_view, std::unique_ptr<decoded_type_data>> decoded_types_map;
  decoded_types_map.reserve(json_array.size());

  for (const fc::variant& type_data : json_array)
  {
    if (!type_data.is_array())
      FC_THROW_EXCEPTION( fc::invalid_arg_exception, "Json with decoded types data is not valid. Expecting a json array inside first array. ${decoded_types_data_json}", (decoded_types_data_json));

    const std::vector<fc::variant>& data_array = type_data.get_array();
    std::unique_ptr<decoded_type_data> loaded_decoded_type;

    try
    {
      auto type_is_reflected = data_array[0].as_int64();
      if (type_is_reflected != 0 && type_is_reflected != 1)
        FC_THROW_EXCEPTION(fc::invalid_arg_exception, "Expecting 0 or 1  on 1th position of json. (Type is reflected or not)");
      if (type_is_reflected)
        loaded_decoded_type = std::make_unique<reflected_decoded_type_data>(data_array);
      else
        loaded_decoded_type = std::make_unique<decoded_type_data>(data_array);
    }
    catch (fc::exception& e)
    {
      FC_THROW_EXCEPTION( fc::invalid_arg_exception, "Json with decoded types data is not valid. Error: ${error}. Part of json with problem: ${data_array}, whole json: ${decoded_types_data_json}", ("error", e.to_detail_string())(data_array)(decoded_types_data_json));
    }
    catch(...)
    {
      FC_THROW("Unspecified error occured during creation of reflected decoded_type_data from json ${data_array}", (data_array));
    }

    decoded_types_map.try_emplace(loaded_decoded_type->get_type_id(), std::move(loaded_decoded_type));
  }
  return decoded_types_map;
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

  for (const auto& [dtdm_key, dtdm_ptr] : decoded_types_data_map)
  {
    if (loaded_decoded_types_map.find(dtdm_key) == loaded_decoded_types_map.end())
    {
      if (no_difference_detected)
        no_difference_detected = false;

      error_ss << "Type is in current decoded types map but not in loaded decoded types map: ";
      if (dtdm_ptr->is_reflected())
        error_ss << dynamic_cast<hive::chain::util::reflected_decoded_type_data *>(dtdm_ptr.get())->get_type_name() << "\n";
      else
        error_ss << dtdm_ptr->get_type_id() << "\n";
    }
    else
    {
      if (const auto* ldtm_ptr = loaded_decoded_types_map.at(dtdm_key).get();
          dtdm_ptr->get_checksum() != ldtm_ptr->get_checksum())
      {
        if (no_difference_detected)
          no_difference_detected = false;

        if (dtdm_ptr->is_reflected())
          error_ss << "Reflected type: " << dynamic_cast<hive::chain::util::reflected_decoded_type_data*>(dtdm_ptr.get())->get_type_name();
        else
          error_ss << "Type with id: " << dtdm_key;

        error_ss << " has checksum: " << dtdm_ptr->get_checksum() << ", which diffs from loaded type: " << ldtm_ptr->get_checksum() << "\n";
      }
    }

    loaded_decoded_types_map.erase(dtdm_key);
  }

  for (const auto& [ldtm_key, ldtm_ptr] : loaded_decoded_types_map)
  {
    if (no_difference_detected)
      no_difference_detected = false;

    error_ss << "Type is in loaded decoded types map but not in current decoded types map:";

    if (ldtm_ptr->is_reflected())
      error_ss << dynamic_cast<hive::chain::util::reflected_decoded_type_data *>(ldtm_ptr.get())->get_type_name() << "\n";
    else
      error_ss << ldtm_ptr->get_type_id() << "\n";
  }

  return std::pair<bool, std::string>(no_difference_detected, error_ss.str());
}

} } } // hive::chain::util