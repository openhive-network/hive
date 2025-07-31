#include "fc/optional.hpp"
#include "fc/static_variant.hpp"
#include "fc/string.hpp"
#include "fc/variant_object.hpp"
#include <cstdint>
#include <hive/protocol/config.hpp>
#include <limits>
#include <string>
#include <vector>

namespace appbase {
namespace swagger_schema {

enum class Type { string, integer, boolean, object, array };

struct Info {
  const fc::string title{"Config and Options description"};
  const fc::string description{"Description of the API"};
  const fc::string version{"0.0.0"};
};

struct ArrayItemsDescription {
  Type type;
};

using default_value_t = fc::static_variant<
        bool,
        std::vector<fc::string>,
        fc::string,
        int8_t, int16_t, int32_t, int64_t,
        uint8_t, uint16_t, uint32_t, uint64_t
      >;

struct TypeDescription {
  Type type;
  fc::string description;
  fc::optional<default_value_t> default_value;
  fc::optional<int64_t> minimum;
  fc::optional<uint64_t> maximum;
  fc::optional<fc::string> format;
  fc::optional<fc::string> pattern;
  fc::optional<ArrayItemsDescription> items;
  fc::string example;

  inline TypeDescription(
      const fc::string &name, const Type type,
      const fc::string &description = "",
      const fc::string &example_value = "",
      const fc::optional<default_value_t> &default_value = fc::optional<default_value_t>(),
      const fc::optional<fc::string> &pattern = fc::optional<fc::string>(),
      const fc::optional<int64_t> &minimum = fc::optional<int64_t>(),
      const fc::optional<uint64_t> &maximum = fc::optional<uint64_t>(),
      const fc::optional<fc::string> &format = fc::optional<fc::string>(),
      const fc::optional<ArrayItemsDescription> &items =fc::optional<ArrayItemsDescription>()
    ) : type(type), description(description), default_value{default_value}, minimum(minimum), maximum(maximum),
      format(format), pattern(pattern), items(items), example(""),
      example_value(example_value), example_name(name) {};

  template <typename T>
  static TypeDescription integer_type(const std::string &name,
                                      const fc::string &description, const fc::optional<T> default_value = fc::optional<T>()) {

    return TypeDescription{
        name,
        Type::integer,
        description,
        "1",
        default_value,
        {},
        std::numeric_limits<T>::min(),
        std::numeric_limits<T>::max(),
    };
  }

static inline TypeDescription string_type(
      const std::string &name, const fc::string &description, const fc::string &example_value,
      const fc::optional<fc::string> &pattern = fc::optional<fc::string>(), fc::optional<fc::string> default_value = fc::optional<fc::string>()) {
  return TypeDescription{name, Type::string, description, example_value, default_value, pattern};
}
static inline TypeDescription datetime_type(const std::string &name,
                                       const fc::string &description, const fc::string &example_value) {
  return TypeDescription{name, Type::string, description, example_value, {},{}, {}, {},"date-time"};
}
static inline TypeDescription array_type(const Type &item_type,
                                    const std::string &name,
                                    const fc::string &description, const fc::string &example_value){
  return TypeDescription{name, Type::array, description, example_value, {}, {}, {}, {}, {}, ArrayItemsDescription{item_type}};
}
static inline TypeDescription boolean_type(const std::string &name,
                                      const fc::string &description, bool default_value) {
  return TypeDescription{name, Type::boolean, description, "", default_value};
}

  void generate_example_for_config();
  void generate_example_for_arguments();
  void generate_example_for_both();

private:
  fc::string example_value;
  fc::string example_name;
};

using Members = fc::map<fc::string, TypeDescription>;

struct StructDefinition {
  const Type type = Type::object;
  Members properties{};
  std::vector<fc::string> required{};
};

struct ComponentsSchemas {
  StructDefinition common{};
  StructDefinition arguments{};
  StructDefinition config{};

  void fill_common();
};

struct Components {
  ComponentsSchemas schemas{};
};

struct OpenAPI {
  const fc::string openapi{"3.1.0"};
  const Info info{};
  Components components{};
};

} // namespace swagger_schema
} // namespace appbase
