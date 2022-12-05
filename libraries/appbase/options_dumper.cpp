#include <appbase/options_dumper.hpp>

#include <boost/any.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <fc/io/json.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/variant.hpp>
#include <fc/variant_object.hpp>

#include <algorithm>
#include <initializer_list>
#include <optional>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>


namespace boost {
  template<>
  std::string lexical_cast<std::string, bool>(const bool& b) {
    std::ostringstream ss;
    ss << std::boolalpha << b;
    return ss.str();
  }
};

namespace appbase {

namespace detail {

template<typename>
struct is_std_vector : std::false_type {};

template<typename T, typename A>
struct is_std_vector<std::vector<T,A>> : std::true_type {};

template <typename... Arg>
inline bool any_of(Arg... arg)
{
  return (arg || ...);
}

inline bool is_multitoken(const boost::program_options::value_semantic* semantic)
{
  return semantic->max_tokens() > 1;
}

template <typename T, typename... T1>
inline static bool try_handle_type_impl(options_dumper::value_info& info, 
  const boost::program_options::value_semantic* option, 
  const char* type_name)
{
  auto* typed_option = dynamic_cast<const boost::program_options::typed_value<T>*>(option);
  constexpr bool is_vector = is_std_vector<T>::value;

  if (!typed_option) {
    if constexpr (sizeof...(T1) > 0)
      return try_handle_type_impl<T1...>(info, option, type_name);
    else
      return false;
  }

  boost::any def_value_any;
  typed_option->apply_default(def_value_any);

  fc::variant def_value;
  if (!def_value_any.empty()) {
    if constexpr (is_vector) {
      std::vector<std::string> def_value_vec;

      for (const auto& v : boost::any_cast<T>(def_value_any)) {
        def_value_vec.push_back(boost::lexical_cast<std::string>(v));
      }

      def_value = std::move(def_value_vec);
    }
    else {
      def_value = boost::lexical_cast<std::string>(boost::any_cast<T>(def_value_any));
    }
  } else {
    if constexpr (is_vector)
      def_value = std::vector<std::string>();
    else
      def_value = std::string();
  }

  info.composed = typed_option->is_composing();
  info.multiple_allowed = typed_option->max_tokens() > 1;
  info.value_type = type_name;
  info.default_value = def_value;

  if constexpr (is_vector) {
    info.value_type += "_array";
  }

  return true;
}

template <typename... T>
static bool try_handle_type(options_dumper::value_info& info, 
  const boost::program_options::value_semantic* option, 
  const char* type_name, bool multitoken)
{
  if (multitoken)
    return try_handle_type_impl<std::vector<T>...>(info, option, type_name);
  
  return try_handle_type_impl<T...>(info, option, type_name)
    || try_handle_type_impl<std::vector<T>...>(info, option, type_name);
}

};

options_dumper::options_dumper() {}

options_dumper::options_dumper(std::initializer_list<entry_t> entries) 
  : _option_groups(std::move(entries)) {}

void options_dumper::add_options_group(const std::string& name, const bpo& group)
{
  _option_groups.emplace(name, group);
}

void options_dumper::clear()
{
  _option_groups.clear();
}

std::string options_dumper::dump_to_string() const
{
  fc::mutable_variant_object obj;

  for (const auto& group : _option_groups)
  {
    std::vector<option_entry> entries;

    for (const auto& option : group.second.get().options()) {
      entries.emplace_back(serialize_option(*option));
    }

    obj[group.first] = std::move(entries);
  }

  return fc::json::to_pretty_string(obj);
}

auto options_dumper::get_value_info(
const boost::program_options::value_semantic* semantic) -> std::optional<value_info>
{
  auto* typed = dynamic_cast<const boost::program_options::typed_value_base*>(semantic);
  if (!typed)
    return std::nullopt;

  value_info info;
  bool multitoken = detail::is_multitoken(semantic);

  bool success = detail::any_of(
    detail::try_handle_type<bool>(info, semantic, "bool", multitoken),
    detail::try_handle_type<int16_t>(info, semantic, "short", multitoken),
    detail::try_handle_type<int32_t>(info, semantic, "int", multitoken),
    detail::try_handle_type<int64_t>(info, semantic, "long", multitoken),
    detail::try_handle_type<uint16_t>(info, semantic, "ushort", multitoken),
    detail::try_handle_type<uint32_t>(info, semantic, "uint", multitoken),
    detail::try_handle_type<uint64_t>(info, semantic, "ulong", multitoken),
    detail::try_handle_type<std::string, fc::string>(info, semantic, "string", multitoken),
    detail::try_handle_type<boost::filesystem::path>(info, semantic, "path", multitoken)
  );

  if (success)
    return info;

  info.composed = semantic->is_composing();
  info.default_value = std::string("");
  info.multiple_allowed = multitoken;
  info.value_type = "unknown";
  return info;
}

auto options_dumper::serialize_option(
const boost::program_options::option_description& option) -> option_entry
{
  const boost::program_options::value_semantic* semantic = option.semantic().get();

  return {
    option.long_name(),
    option.description(),
    semantic->is_required(),
    get_value_info(semantic),
  };
}

};

FC_REFLECT(appbase::options_dumper::value_info, (multiple_allowed)(composed)(value_type)(default_value));
FC_REFLECT(appbase::options_dumper::option_entry, (name)(description)(required)(value));
