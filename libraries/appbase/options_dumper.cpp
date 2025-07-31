#include "fc/optional.hpp"
#include "fc/reflect/reflect.hpp"
#include "fc/string.hpp"
#include <algorithm>
#include <appbase/options_dumper.hpp>
#include <appbase/swagger_schema.hpp>

#include <boost/any.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/lexical_cast.hpp>

#include <cstdint>
#include <fc/exception/exception.hpp>
#include <fc/io/json.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/variant_object.hpp>

#include <sstream>
#include <utility>
#include <vector>

namespace appbase {

swagger_schema::TypeDescription
try_handle_type(const boost::program_options::option_description &option,
                const boost::program_options::typed_value_base *typed) {
  const std::set<std::string> _fixed_names_for_path{};
  /*
      "wallet-dir",
      "log-json-rpc",
      "data-dir",
      "config",
      "account-history-rocksdb-path",
      "shared-file-dir",
      "comments-rocksdb-path",
      "pacemaker-source",
      "snapshot-root-dir"};
  */
  const auto path_regex = R"(^(.+)\/([^\/]+)$)";
  const auto &name = option.long_name();
  const auto &description = option.description();

  boost::any value;
  bool is_default_available = option.semantic()->apply_default(value);
  if (typed->value_type() == typeid(bool)) {
    return swagger_schema::TypeDescription::boolean_type(
        name, description, boost::any_cast<bool>(value));
  } else if (typed->value_type() == typeid(int8_t))
    return swagger_schema::TypeDescription::integer_type<uint8_t>(
        name, description,
        (is_default_available ? boost::any_cast<int8_t>(value)
                              : fc::optional<int8_t>()));
  else if (typed->value_type() == typeid(int16_t))
    return swagger_schema::TypeDescription::integer_type<int16_t>(
        name, description,
        (is_default_available ? boost::any_cast<int16_t>(value)
                              : fc::optional<int16_t>()));
  else if (typed->value_type() == typeid(int32_t))
    return swagger_schema::TypeDescription::integer_type<int32_t>(
        name, description,
        (is_default_available ? boost::any_cast<int32_t>(value)
                              : fc::optional<int32_t>()));
  else if (typed->value_type() == typeid(int64_t))
    return swagger_schema::TypeDescription::integer_type<int64_t>(
        name, description,
        (is_default_available ? boost::any_cast<int64_t>(value)
                              : fc::optional<int64_t>()));
  else if (typed->value_type() == typeid(uint8_t))
    return swagger_schema::TypeDescription::integer_type<uint8_t>(
        name, description,
        (is_default_available ? boost::any_cast<uint8_t>(value)
                              : fc::optional<uint8_t>()));
  else if (typed->value_type() == typeid(uint16_t))
    return swagger_schema::TypeDescription::integer_type<uint16_t>(
        name, description,
        (is_default_available ? boost::any_cast<uint16_t>(value)
                              : fc::optional<uint16_t>()));
  else if (typed->value_type() == typeid(uint32_t))
    return swagger_schema::TypeDescription::integer_type<uint32_t>(
        name, description,
        (is_default_available ? boost::any_cast<uint32_t>(value)
                              : fc::optional<uint32_t>()));
  else if (typed->value_type() == typeid(uint64_t))
    return swagger_schema::TypeDescription::integer_type<uint64_t>(
        name, description,
        (is_default_available ? boost::any_cast<uint64_t>(value)
                              : fc::optional<uint64_t>()));
  else if (typed->value_type() == typeid(std::vector<std::string>))
    return swagger_schema::TypeDescription::array_type(
        swagger_schema::Type::string, name, description, "value");
  else {
    return swagger_schema::TypeDescription::string_type(
        name, description, "value", path_regex,
        (is_default_available
             ? fc::optional<fc::string>(boost::any_cast<fc::string>(value))
             : fc::optional<fc::string>()));
  }
  FC_ASSERT(false, "Unknown type for option: ${name} (${type})",
            ("name", name)("type", typed->value_type().name()));
}

options_dumper::options_dumper(std::initializer_list<entry_t> entries)
    : option_groups(std::move(entries)) {}

swagger_schema::TypeDescription
get_value_info(const boost::program_options::option_description &option) {
  const auto semantic = option.semantic();
  const std::string &name = option.long_name();

  if (!semantic)
    return swagger_schema::TypeDescription::string_type(
        name, option.description(), "value");

  auto *_typed = dynamic_cast<const boost::program_options::typed_value_base *>(
      semantic.get());
  if (!_typed)
    return swagger_schema::TypeDescription::string_type(
        name, option.description(), "value");

  return try_handle_type(option, _typed);
}

swagger_schema::Members::value_type
serialize_option(const boost::program_options::option_description &option) {
  return std::make_pair(option.long_name(), get_value_info(option));
}

std::string options_dumper::dump_to_string() const {
  auto arguments_in = option_groups.find("command_line");
  auto config_in = option_groups.find("config_file");

  FC_ASSERT(arguments_in != option_groups.end(),
            "Command line options group is not found");
  FC_ASSERT(config_in != option_groups.end(),
            "Config file options group is not found");

  swagger_schema::OpenAPI schema{};

  swagger_schema::Members &config_out =
      schema.components.schemas.config.properties;
  swagger_schema::Members &arguments_out =
      schema.components.schemas.arguments.properties;

  using fixed_options_t =
      std::pair<options_group_t::const_iterator, swagger_schema::Members &>;
  const std::list<fixed_options_t> _fixed_options = {
      {config_in, config_out}, {arguments_in, arguments_out}};

  for (const fixed_options_t &group : _fixed_options)
    for (const auto &option : group.first->second.options())
      group.second.emplace(serialize_option(*option));

  schema.components.schemas.fill_common();
  return fc::json::to_pretty_string(schema);
}

namespace swagger_schema {

void ComponentsSchemas::fill_common() {
  // Fill common properties with intersection of arguments and config properties
  this->arguments.required.reserve(this->arguments.properties.size());
  for (auto &arg : this->arguments.properties) {
    this->arguments.required.emplace_back(arg.first);
    arg.second.generate_example_for_arguments();
  }

  this->config.required.reserve(this->arguments.properties.size());
  for (auto &cfg : this->config.properties) {
    this->config.required.emplace_back(cfg.first);
    cfg.second.generate_example_for_config();
  }
  return;

  std::set_intersection(
      this->arguments.properties.begin(), this->arguments.properties.end(),
      this->config.properties.begin(), this->config.properties.end(),
      std::inserter(this->common.properties, this->common.properties.begin()),
      [](const auto &a, const auto &b) { return a.first < b.first; });

  // Remove common properties from arguments and config
  for (auto &common_property : this->common.properties) {
    this->arguments.properties.erase(common_property.first);
    this->config.properties.erase(common_property.first);

    common_property.second.generate_example_for_both();
  }

  for (auto &argument_property : this->arguments.properties) {
    if (this->common.properties.find(argument_property.first) !=
        this->common.properties.end())
      this->common.properties.at(argument_property.first)
          .generate_example_for_arguments();
  }

  for (auto &config_property : this->config.properties) {
    if (this->common.properties.find(config_property.first) !=
        this->common.properties.end())
      this->common.properties.at(config_property.first)
          .generate_example_for_config();
  }
}

void TypeDescription::generate_example_for_config() {
  const auto config_entry = this->example_name + " = ";
  std::stringstream ss;

  if (this->type == Type::array) {
    for (int i = 0; i < 2; ++i)
      ss << config_entry << this->example_value << i << std::endl;
  } else {
    ss << config_entry << this->example_value << std::endl;
  }

  this->example = ss.str();
}

void TypeDescription::generate_example_for_arguments() {
  auto argument_name = "--" + this->example_name + " ";
  std::stringstream ss;

  if (this->type == Type::array) {
    for (int i = 0; i < 2; ++i)
      ss << argument_name << this->example_value << i << " " << std::endl;
  } else {
    ss << argument_name << this->example_value << std::endl;
  }

  this->example = ss.str();
}

void TypeDescription::generate_example_for_both() {
  std::stringstream ss;
  ss << "Config usage:" << std::endl;
  ;
  generate_example_for_config();
  ss << this->example << std::endl << std::endl;
  ss << "Command line usage:" << std::endl;
  generate_example_for_arguments();
  ss << this->example << std::endl;
  this->example = ss.str();
}

} // namespace swagger_schema

}; // namespace appbase

FC_REFLECT_ENUM(appbase::swagger_schema::Type,
                (string)(integer)(boolean)(object)(array))
FC_REFLECT(appbase::swagger_schema::ArrayItemsDescription, (type))
FC_REFLECT(
    appbase::swagger_schema::TypeDescription,
    (type)(description)(default_value)(example)(format)(items)(minimum)(maximum)(pattern))
FC_REFLECT(appbase::swagger_schema::StructDefinition,
           (type)(properties)(required))
FC_REFLECT(appbase::swagger_schema::ComponentsSchemas,
           (common)(arguments)(config))
FC_REFLECT(appbase::swagger_schema::Components, (schemas))
FC_REFLECT(appbase::swagger_schema::Info, (title)(description)(version))
FC_REFLECT(appbase::swagger_schema::OpenAPI, (openapi)(info)(components))
