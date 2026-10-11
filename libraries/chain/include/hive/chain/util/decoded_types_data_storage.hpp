#pragma once

#include <chainbase/allocators.hpp>
#include <chainbase/util/object.hpp>

#include <fc/crypto/ripemd160.hpp>
#include <fc/crypto/sha256.hpp>
#include <fc/exception/exception.hpp>
#include <fc/io/json.hpp>
#include <fc/reflect/reflect.hpp>
#include <fc/static_variant.hpp>

#include <hive/protocol/fixed_string.hpp>

#include <map>
#include <optional>
#include <type_traits>
#include <unordered_set>

#include <boost/mpl/vector.hpp>
#include <boost/mpl/for_each.hpp>

namespace hive { namespace chain { namespace util {

std::string calculate_checksum_from_string(const std::string_view str);

struct decoded_type_data
{
  struct member_data
  {
      std::string type;
      std::string name;
      size_t offset;
  };

  using members_vector_t = std::vector<member_data>;
  using enum_values_vector_t = std::vector<std::pair<std::string, size_t>>;

  decoded_type_data() = delete;

  decoded_type_data(const std::string_view _checksum, const std::string_view _type_name, const size_t _type_size, const size_t type_align);
  decoded_type_data(const std::string_view _checksum, const std::string_view _type_name, enum_values_vector_t&& _enum_values);
  decoded_type_data(const std::string_view _checksum, const std::string_view _type_name, const size_t _type_size, const size_t type_align,
                    members_vector_t&& _members);

  // json pattern: "{"reflected": bool, "type_id": string, "checksum": string, "size_of": int, "align_of": int}"
  // json pattern: "{"reflected": bool, "type_id": string, "checksum": string, "name": string, "size_of": int, "align_of": int, "members": [{"type":string, "name":string, "offset":int}, ...}]}"
  // json pattern: "{"reflected": bool, "type_id": string, "checksum": string, "name": string, "enum_values": [["enum",int], ...]}"
  decoded_type_data(const std::string& json);

  fc::optional<members_vector_t> members; // in case of structure
  fc::optional<enum_values_vector_t> enum_values; // in case of enum
  fc::optional<size_t> size_of;
  fc::optional<size_t> align_of;
  std::string name;
  std::string checksum;
  bool reflected;
};

/*
  Describes how a state type is serialized by fc::raw (snapshot format), independently of its memory layout:
  no sizeof, alignof or offsets, and type names are canonical (see canonical_type_name).
  Meaning of entries depends on kind:
  - "struct" - reflected members in declaration order: member name, canonical member type,
  - "enum" - enum values: value name, numeric value,
  - "static_variant" - alternatives in order: index, canonical alternative type,
  - "container" - first entry {"container", container kind}, then element types ("element", or "key" and "mapped" for maps).
*/
struct serialized_type_data
{
  struct entry
  {
    std::string name;
    std::string value;

    bool operator==(const entry& other) const { return name == other.name && value == other.value; }
  };

  using entries_vector_t = std::vector<entry>;

  std::string name;
  std::string kind;
  entries_vector_t entries;

  bool operator==(const serialized_type_data& other) const { return name == other.name && kind == other.kind && entries == other.entries; }
};

namespace canonical_names
{
  /* Replaces standard library implementation namespaces (std::__cxx11::, std::__1::) with std::. */
  std::string normalize_type_name(std::string type_name);
  /* Name of the template of a demangled template instance name, i.e. without its trailing argument list. */
  std::string template_name(const std::string& demangled_type_name);
}

/*
  Name of T which is the same for the same source regardless of Boost version, standard library and platform:
  allocator and segment manager template arguments are skipped, containers (see container_description) are named by their kind and
  element types only - without comparator and options, and integer types are named by their width.
*/
template <typename T>
std::string canonical_type_name();

namespace canonical_names
{
  template <typename T, typename = std::void_t<>>
  struct is_allocation_argument : std::false_type {};

  /* Allocators and segment managers - they only define where data is held, not what is serialized. */
  template <typename T>
  struct is_allocation_argument<T, std::void_t<decltype(std::declval<T&>().allocate(std::size_t{}))>> : std::true_type {};

  template <typename T>
  struct container_description : std::false_type {};

  template <typename T, typename... Rest>
  struct container_description<boost::container::vector<T, Rest...>> : std::true_type
  {
    static constexpr const char* kind = "boost::container::vector";
    static serialized_type_data::entries_vector_t elements() { return {{"element", canonical_type_name<T>()}}; }
  };

  template <typename T, typename... Rest>
  struct container_description<boost::container::deque<T, Rest...>> : std::true_type
  {
    static constexpr const char* kind = "boost::container::deque";
    static serialized_type_data::entries_vector_t elements() { return {{"element", canonical_type_name<T>()}}; }
  };

  template <typename T, typename... Rest>
  struct container_description<boost::container::flat_set<T, Rest...>> : std::true_type
  {
    static constexpr const char* kind = "boost::container::flat_set";
    static serialized_type_data::entries_vector_t elements() { return {{"element", canonical_type_name<T>()}}; }
  };

  template <typename K, typename V, typename... Rest>
  struct container_description<boost::container::flat_map<K, V, Rest...>> : std::true_type
  {
    static constexpr const char* kind = "boost::container::flat_map";
    static serialized_type_data::entries_vector_t elements() { return {{"key", canonical_type_name<K>()}, {"mapped", canonical_type_name<V>()}}; }
  };

  template <typename C, typename... Rest>
  struct container_description<boost::container::basic_string<C, Rest...>> : std::true_type
  {
    static constexpr const char* kind = "boost::container::basic_string";
    static serialized_type_data::entries_vector_t elements() { return {{"element", canonical_type_name<C>()}}; }
  };

  template <typename T, typename... Rest>
  struct container_description<boost::multi_index::multi_index_container<T, Rest...>> : std::true_type
  {
    static constexpr const char* kind = "boost::multi_index::multi_index_container";
    static serialized_type_data::entries_vector_t elements() { return {{"element", canonical_type_name<T>()}}; }
  };

  template <typename T, typename... Rest>
  struct container_description<std::vector<T, Rest...>> : std::true_type
  {
    static constexpr const char* kind = "std::vector";
    static serialized_type_data::entries_vector_t elements() { return {{"element", canonical_type_name<T>()}}; }
  };

  template <typename C, typename... Rest>
  struct container_description<std::basic_string<C, Rest...>> : std::true_type
  {
    static constexpr const char* kind = "std::basic_string";
    static serialized_type_data::entries_vector_t elements() { return {{"element", canonical_type_name<C>()}}; }
  };

  template <typename T>
  struct fixed_size_array : std::false_type {};

  template <typename T, size_t N>
  struct fixed_size_array<fc::array<T, N>> : std::true_type
  {
    static constexpr const char* kind = "fc::array";
    using element_type = T;
    static constexpr size_t size = N;
  };

  template <typename T, size_t N>
  struct fixed_size_array<fc::int_array<T, N>> : std::true_type
  {
    static constexpr const char* kind = "fc::int_array";
    using element_type = T;
    static constexpr size_t size = N;
  };

  template <typename Arg>
  void append_template_argument_name(std::vector<std::string>& names)
  {
    if constexpr (!is_allocation_argument<Arg>::value)
      names.push_back(canonical_type_name<Arg>());
  }

  template <typename T>
  struct template_arguments : std::false_type {};

  template <template <typename...> typename T, typename... Args>
  struct template_arguments<T<Args...>> : std::true_type
  {
    static std::vector<std::string> names()
    {
      std::vector<std::string> result;
      ( append_template_argument_name<Args>(result), ... );
      return result;
    }
  };

  std::string join_template_arguments(const std::string& template_name, const std::vector<std::string>& arguments);

  template <typename T>
  std::vector<std::string> element_type_names()
  {
    std::vector<std::string> names;
    for (const auto& element : container_description<T>::elements())
      names.push_back(element.value);
    return names;
  }
}

template <typename T>
std::string canonical_type_name()
{
  using namespace canonical_names;

  if constexpr (!std::is_same_v<T, std::remove_cv_t<T>>)
    return canonical_type_name<std::remove_cv_t<T>>();
  else if constexpr (std::is_same_v<T, bool>)
    return "bool";
  else if constexpr (std::is_same_v<T, char>)
    return "char";
  else if constexpr (std::is_same_v<T, __int128_t>)
    return "int128_t";
  else if constexpr (std::is_same_v<T, __uint128_t>)
    return "uint128_t";
  else if constexpr (std::is_integral_v<T>)
    return std::string(std::is_signed_v<T> ? "int" : "uint") + std::to_string(sizeof(T) * 8) + "_t";
  else if constexpr (container_description<T>::value)
    return join_template_arguments(container_description<T>::kind, element_type_names<T>());
  else if constexpr (fixed_size_array<T>::value)
    return join_template_arguments(fixed_size_array<T>::kind,
      {canonical_type_name<typename fixed_size_array<T>::element_type>(), std::to_string(fixed_size_array<T>::size)});
  else if constexpr (template_arguments<T>::value)
    return join_template_arguments(template_name(boost::core::demangle(typeid(T).name())), template_arguments<T>::names());
  else
    return normalize_type_name(boost::core::demangle(typeid(T).name()));
}

namespace decoders
{
  /* Main decoder - distinguish which type is reflected or not. Begins decoding process. */
    template<typename T, bool is_defined = fc::reflector<T>::is_defined::value>
    class main_decoder;

    namespace non_reflected_types
    {
      /* For all non reflected types which request specific decoder. */
      template<typename ...Args>
      struct specific_type_decoder;
    }
}

enum class state_definitions_verification_result : uint8_t
{
  state_definitions_matches = 0,
  mismatch_log_warning = 1,
  mismatch_throw_error = 2
};

class decoded_types_data_storage final
{
  private:
    bool add_type_to_decoded_types_set(const std::string_view type_id);
    void add_decoded_type_data_to_map(decoded_type_data&& decoded_type);
    void add_serialized_type_data_to_map(serialized_type_data&& serialized_type);

    template <typename T, bool> friend class decoders::main_decoder;
    template <typename ...Args> friend class decoders::non_reflected_types::specific_type_decoder;

  public:
    // stores data about decoded types.
    using decoded_types_map_t = std::map<std::string, decoded_type_data>;
    // stores serialization descriptors of decoded types, by canonical type name.
    using serialized_types_map_t = std::map<std::string, serialized_type_data>;
    decoded_types_data_storage() = default;
    decoded_types_data_storage(const std::string& decoded_types_data_json);
    ~decoded_types_data_storage();

    template <typename T>
    void register_new_type()
    {
      decoders::main_decoder<T> decoder;
      decoder.decode(*this);
    }

    template <typename T>
    const decoded_type_data& get_decoded_type_data() const
    {
      auto it = decoded_types_data_map.find(boost::core::demangle(typeid(T).name()));

      if (it == decoded_types_data_map.end())
        FC_THROW_EXCEPTION( fc::key_not_found_exception, "Cound not find decoded type data. Type id: ${name} .", ("name", std::string(typeid(T).name())) );

      return it->second;
    }

    template <typename T>
    std::string_view get_decoded_type_checksum() const
    {
      return get_decoded_type_data<T>().checksum;
    }

    template <typename T>
    const serialized_type_data& get_serialized_type_data() const
    {
      const std::string name = canonical_type_name<T>();
      auto it = serialized_types_data_map.find(name);

      if (it == serialized_types_data_map.end())
        FC_THROW_EXCEPTION( fc::key_not_found_exception, "Could not find serialized type data. Type: ${name} .", (name) );

      return it->second;
    }

    const decoded_types_map_t& get_decoded_types_data_map() const { return decoded_types_data_map; }
    const serialized_types_map_t& get_serialized_types_data_map() const { return serialized_types_data_map; }

    // if json string is specified, generate pretty string with info from it, otherwise generate output from map with current decoded types data.
    std::string generate_decoded_types_data_pretty_string(const std::string& decoded_types_data_json = std::string()) const;
    std::string generate_decoded_types_data_json_string() const;

    // state_definitions_verification_result - comparing result, in case of mismatch_throw_error - exception should be thrown , string - details of detected differences.
    std::pair<state_definitions_verification_result, std::string> check_if_decoded_types_data_json_matches_with_current_decoded_data(const std::string& decoded_types_data_json) const;

    std::string generate_serialized_types_data_json_string() const;
    // like generate_decoded_types_data_pretty_string, for serialization descriptors.
    std::string generate_serialized_types_data_pretty_string(const std::string& serialized_types_data_json = std::string()) const;
    // Like check_if_decoded_types_data_json_matches_with_current_decoded_data, but compares serialization descriptors. Details name the first differing entry of each differing type.
    std::pair<state_definitions_verification_result, std::string> check_if_serialized_types_data_json_matches_with_current_serialized_data(const std::string& serialized_types_data_json) const;

  private:
    using decoding_types_set_t = std::unordered_set<std::string>;
    decoding_types_set_t decoded_types_set; // Set is used to kept info about types which are decoded or DURING decoding process.
    decoded_types_map_t decoded_types_data_map; //Storing info about decoded types.
    serialized_types_map_t serialized_types_data_map;
};

namespace decoders
{
  /* Checks if T is a template type. IMPORTANT - doesn't work for template types which accepts value, for example fc::array<type, value>, ::value will return false. */
  template <typename>
  struct template_types_detector : std::false_type
  {
    template_types_detector(decoded_types_data_storage&) {}
    void analyze_arguments() {}
  };

  struct type_analyzer
  {
    type_analyzer(decoded_types_data_storage& _dtds) : dtds(_dtds) {}

    template<typename U> void operator()(U*)
    {
      main_decoder<U> decoder;
      decoder.decode(dtds);
    }

    private:
      decoded_types_data_storage& dtds;
  };

  template <template <typename...> typename T, typename... Args>
  struct template_types_detector<T<Args...>> : std::bool_constant<sizeof...(Args) != 0U>
  {
    private:
      decoded_types_data_storage& dtds;

    public:
      template_types_detector(decoded_types_data_storage& _dtds) : dtds(_dtds) {}

      void analyze_arguments()
      {
        typedef boost::mpl::vector<Args...> types_vector;
        type_analyzer analyzer(dtds);
        boost::mpl::for_each<typename boost::mpl::transform<types_vector, boost::add_pointer<boost::mpl::_1>>::type>(analyzer);
      }
  };

  /* Tools for decoding reflected types and enums. */
  template <typename T>
  class visitor_type_decoder
  {
    public:
      visitor_type_decoder(decoded_types_data_storage& _dtds) : dtds(_dtds)
      {
        const std::string type_id = typeid(T).name();
        type_size_of = sizeof(T);
        type_align_of = alignof(T);
        const std::string size_str = std::to_string(type_size_of);
        const std::string align_str = std::to_string(type_align_of);
        encoder.write(type_id.data(), type_id.size());
        encoder.write(size_str.data(), size_str.size());
        encoder.write(align_str.data(), align_str.size());
      }

      template <typename Member, class Class>
      void operator()(Member Class::*member, const char *name) const
      {
        decoders::main_decoder<Member> decoder;
        decoder.decode(dtds);

        if (template_types_detector<Member>::value)
        {
          template_types_detector<Member> detector(dtds);
          detector.analyze_arguments();
        }

        const std::string field_name(name);
        const std::string type_id(typeid(Member).name());
        Class* const nullObj = nullptr;
        const size_t member_offset = size_t(static_cast<void*>(&(nullObj->*member)));
        const std::string member_offset_str = std::to_string(member_offset);

        encoder.write(type_id.data(), type_id.size());
        encoder.write(field_name.data(), field_name.size());
        encoder.write(member_offset_str.data(), member_offset_str.size());
        members.push_back({.type = boost::core::demangle(typeid(Member).name()), .name = field_name, .offset = member_offset});
        serialized_members.push_back({.name = field_name, .value = canonical_type_name<Member>()});
      }

      decoded_type_data get_decoded_type_data()
      {
        return decoded_type_data(encoder.result().str(), boost::core::demangle(typeid(T).name()), type_size_of, type_align_of, std::move(members));
      }

      serialized_type_data get_serialized_type_data()
      {
        return serialized_type_data{.name = canonical_type_name<T>(), .kind = "struct", .entries = std::move(serialized_members)};
      }

    private:
      mutable fc::ripemd160::encoder encoder;
      mutable decoded_type_data::members_vector_t members;
      mutable serialized_type_data::entries_vector_t serialized_members;
      decoded_types_data_storage& dtds;
      size_t type_size_of;
      size_t type_align_of;
    };

  template <typename T>
  class visitor_enum_decoder
  {
    public:
      visitor_enum_decoder()
      {
        const std::string type_id = typeid(T).name();
        encoder.write(type_id.data(), type_id.size());
      }

      void operator()(const char *name, const int64_t value) const
      {
        const std::string value_name(name);
        const std::string value_in_string(std::to_string(value));
        encoder.write(value_name.data(), value_name.size());
        encoder.write(value_in_string.data(), value_in_string.size());
        enum_values.push_back({value_name, value});
        serialized_values.push_back({.name = value_name, .value = value_in_string});
      }

      decoded_type_data get_decoded_type_data()
      {
        return decoded_type_data(encoder.result().str(), boost::core::demangle(typeid(T).name()), std::move(enum_values));
      }

      serialized_type_data get_serialized_type_data()
      {
        return serialized_type_data{.name = canonical_type_name<T>(), .kind = "enum", .entries = std::move(serialized_values)};
      }

    private:
      mutable fc::ripemd160::encoder encoder;
      mutable decoded_type_data::enum_values_vector_t enum_values;
      mutable serialized_type_data::entries_vector_t serialized_values;
    };

  /* Throws a compilation error in case of instantiation. */
  template <typename T> struct unable_to_decode_type_error;

  /* Allows to check if type is specified template type without parsing template arguments. */
  template<typename U, template<typename...> class Ref>
  struct is_specialization : std::false_type {};

  template<template<typename...> class Ref, typename... Args>
  struct is_specialization<Ref<Args...>, Ref> : std::true_type {};

  template <typename T>
  serialized_type_data describe_container()
  {
    serialized_type_data::entries_vector_t entries{{"container", canonical_names::container_description<T>::kind}};
    for (auto& element : canonical_names::container_description<T>::elements())
      entries.push_back(std::move(element));
    return serialized_type_data{.name = canonical_type_name<T>(), .kind = "container", .entries = std::move(entries)};
  }

  /* Allows to check if non reflected type is decodable by specific decoder. */
  template <typename T, typename = std::void_t<>>
  struct is_specific_type_decodable : std::false_type {};

  template <typename T>
  struct is_specific_type_decodable<T, std::void_t<decltype(non_reflected_types::specific_type_decoder<T>{})>> : std::true_type {};

  /* Decoding starting point. */
  template<typename T>
  struct main_decoder<T, true /* is_defined - type is reflected */>
  {
    void decode(decoded_types_data_storage& dtds) const
    {
      const std::string_view type_id_name = typeid(T).name();

      if (!dtds.add_type_to_decoded_types_set(type_id_name))
          return;
      if constexpr (fc::reflector<T>::is_enum::value)
      {
        visitor_enum_decoder<T> visitor;
        fc::reflector<T>::visit(visitor);
        dtds.add_decoded_type_data_to_map(std::move(visitor.get_decoded_type_data()));
        dtds.add_serialized_type_data_to_map(visitor.get_serialized_type_data());
      }
      else
      {
        visitor_type_decoder<T> visitor(dtds);
        fc::reflector<T>::visit(visitor);
        dtds.add_decoded_type_data_to_map(std::move(visitor.get_decoded_type_data()));
        dtds.add_serialized_type_data_to_map(visitor.get_serialized_type_data());
      }
    }
  };

  template<typename T>
  struct main_decoder<T, false /* is_defined - type is not reflected */>
  {
    void decode(decoded_types_data_storage& dtds) const
    {
      /* Type is fundamental - do nothing. */
      if constexpr (std::is_fundamental<T>::value) {}
      /* Type is trival. */
      else if constexpr (std::is_trivial<T>::value)
      {
        if (template_types_detector<T>::value &&
            dtds.add_type_to_decoded_types_set(typeid(T).name()))
        {
          template_types_detector<T> detector(dtds);
          detector.analyze_arguments();
        }
      }
      /* Type is an index. We want to extract key type from it. */
      else if constexpr (is_specialization<T, boost::multi_index::multi_index_container>::value)
      {
        const std::string index_type_id = typeid(T).name();

        if (dtds.add_type_to_decoded_types_set(index_type_id))
        {
          dtds.add_decoded_type_data_to_map(std::move(decoded_type_data(calculate_checksum_from_string(index_type_id), boost::core::demangle(index_type_id.c_str()), sizeof(T), alignof(T))));
          dtds.add_serialized_type_data_to_map(describe_container<T>());
          main_decoder<typename T::value_type> decoder;
          decoder.decode(dtds);
        }
      }
      /*
        Boost types. Their layout is described only by typeid, sizeof and alignof - Boost version is not checked, and a new Boost version can change
        layout of a container without changing its typeid (e.g. boost::container::deque in Boost 1.90). Their serialization is described by container kind
        and element types.
      */
      else if constexpr (is_specialization<T, boost::interprocess::allocator>::value ||
                         is_specialization<T, boost::container::basic_string>::value ||
                         is_specialization<T, boost::container::flat_map>::value ||
                         is_specialization<T, boost::container::flat_set>::value ||
                         is_specialization<T, boost::container::deque>::value ||
                         is_specialization<T, boost::container::vector>::value)
      {
        const std::string boost_type_id = typeid(T).name();

        if (dtds.add_type_to_decoded_types_set(boost_type_id))
        {
          dtds.add_decoded_type_data_to_map(std::move(decoded_type_data(calculate_checksum_from_string(boost_type_id), boost::core::demangle(boost_type_id.c_str()), sizeof(T), alignof(T))));
          if constexpr (canonical_names::container_description<T>::value)
            dtds.add_serialized_type_data_to_map(describe_container<T>());
        }
      }
      /* Type should be hashable if it's not handled by one of above ifs. */
      else if constexpr (is_specific_type_decodable<T>::value)
      {
        if (dtds.add_type_to_decoded_types_set(typeid(T).name()))
        {
          non_reflected_types::specific_type_decoder<T> decoder;
          decoder.decode(dtds);
        }
      }
      /* If we cannot decode type, compilation error should occur. */
      else
        unable_to_decode_type_error<T> error;
    }
  };

  namespace non_reflected_types
  {
    /* Below are defined decoders for non reflected types which need to be handled separately. */
    template<typename T>
    struct specific_type_decoder<chainbase::oid<T>>
    {
      void decode(decoded_types_data_storage& dtds) const
      {
        std::stringstream ss;
        const std::string type_id = typeid(chainbase::oid<T>).name();
        ss << type_id;
        ss << sizeof(chainbase::oid<T>);
        ss << alignof(chainbase::oid<T>);

        chainbase::oid<T> oid(0);
        using oid_value_type = decltype(oid.get_value());
        ss << sizeof(oid_value_type);
        ss << alignof(oid_value_type);
        ss << typeid(oid_value_type).name();
        dtds.add_decoded_type_data_to_map(std::move(decoded_type_data(calculate_checksum_from_string(ss.str()), boost::core::demangle(type_id.c_str()), sizeof(chainbase::oid<T>), alignof(chainbase::oid<T>))));
        main_decoder<T> decoder;
        decoder.decode(dtds);
      }
    };

    template<typename T>
    struct specific_type_decoder<chainbase::oid_ref<T>>
    {
      void decode(decoded_types_data_storage& dtds) const
      {
        std::stringstream ss;
        const std::string type_id = typeid(chainbase::oid_ref<T>).name();
        ss << type_id;
        ss << sizeof(chainbase::oid_ref<T>);
        ss << alignof(chainbase::oid_ref<T>);

        chainbase::oid_ref<T> oid_ref;
        using oid_value_type = decltype(oid_ref.get_value());
        ss << sizeof(oid_value_type);
        ss << alignof(oid_value_type);
        ss << typeid(oid_value_type).name();

        dtds.add_decoded_type_data_to_map(std::move(decoded_type_data(calculate_checksum_from_string(ss.str()), boost::core::demangle(type_id.c_str()), sizeof(chainbase::oid_ref<T>), alignof(chainbase::oid_ref<T>))));
        main_decoder<T> decoder;
        decoder.decode(dtds);
      }
    };

    template<>
    struct specific_type_decoder<fc::time_point_sec>
    {
      void decode(decoded_types_data_storage& dtds) const
      {
        std::stringstream ss;
        const std::string type_id = typeid(fc::time_point_sec).name();
        ss << type_id;
        ss << sizeof(fc::time_point_sec);
        ss << alignof(fc::time_point_sec);

        fc::time_point_sec tps;
        using sec_since_epoch_type = decltype(tps.sec_since_epoch());
        ss << sizeof(sec_since_epoch_type);
        ss << alignof(sec_since_epoch_type);
        ss << typeid(sec_since_epoch_type).name();

        dtds.add_decoded_type_data_to_map(std::move(decoded_type_data(calculate_checksum_from_string(ss.str()), boost::core::demangle(type_id.c_str()), sizeof(fc::time_point_sec), alignof(fc::time_point_sec))));
      }
    };

    template<typename A, typename B>
    struct specific_type_decoder<fc::erpair<A, B>>
    {
      void decode(decoded_types_data_storage& dtds) const
      {
        std::stringstream ss;
        const std::string type_id = typeid(fc::erpair<A, B>).name();
        ss << type_id;
        ss << sizeof(fc::erpair<A, B>);
        ss << alignof(fc::erpair<A, B>);

        fc::erpair<A, B>* const nullObj = nullptr;
        ss << size_t(static_cast<void*>(&(nullObj->first)));
        ss << typeid(nullObj->first).name();
        ss << size_t(static_cast<void*>(&(nullObj->second)));
        ss << typeid(nullObj->second).name();

        dtds.add_decoded_type_data_to_map(std::move(decoded_type_data(calculate_checksum_from_string(ss.str()), boost::core::demangle(type_id.c_str()), sizeof(fc::erpair<A, B>), alignof(fc::erpair<A, B>))));

        main_decoder<A> decoder_A;
        decoder_A.decode(dtds);
        main_decoder<B> decoder_B;
        decoder_B.decode(dtds);
      }
    };

    template<typename T, size_t N>
    struct specific_type_decoder<fc::array<T, N>>
    {
      void decode(decoded_types_data_storage& dtds) const
      {
        std::stringstream ss;
        const std::string type_id = typeid(fc::array<T, N>).name();
        ss << type_id;
        ss << sizeof(fc::array<T, N>);
        ss << alignof(fc::array<T, N>);

        fc::array<T, N>* const nullObj = nullptr;
        ss << size_t(static_cast<void*>(&(nullObj->data)));
        ss << typeid(nullObj->data).name();

        dtds.add_decoded_type_data_to_map(std::move(decoded_type_data(calculate_checksum_from_string(ss.str()), boost::core::demangle(type_id.c_str()), sizeof(fc::array<T, N>), alignof(fc::array<T, N>))));
        main_decoder<T> decoder;
        decoder.decode(dtds);
      }
    };

    template< typename T>
    struct specific_type_decoder<hive::protocol::fixed_string_impl<T>>
    {
      void decode(decoded_types_data_storage& dtds) const
      {
        std::stringstream ss;
        const std::string type_id = typeid(hive::protocol::fixed_string_impl<T>).name();
        ss << type_id;
        ss << sizeof(hive::protocol::fixed_string_impl<T>);
        ss << alignof(hive::protocol::fixed_string_impl<T>);

        hive::protocol::fixed_string_impl<T>* const nullObj = nullptr;
        ss << size_t(static_cast<void*>(&(nullObj->data)));
        ss << typeid(nullObj->data).name();

        dtds.add_decoded_type_data_to_map(std::move(decoded_type_data(calculate_checksum_from_string(ss.str()), boost::core::demangle(type_id.c_str()), sizeof(hive::protocol::fixed_string_impl<T>), alignof(hive::protocol::fixed_string_impl<T>))));
        main_decoder<T> decoder;
        decoder.decode(dtds);
      }
    };

    template<>
    struct specific_type_decoder<fc::ripemd160>
    {
      void decode(decoded_types_data_storage& dtds) const
      {
        std::stringstream ss;
        const std::string type_id = typeid(fc::ripemd160).name();
        ss << type_id;
        ss << sizeof(fc::ripemd160);
        ss << alignof(fc::ripemd160);

        fc::ripemd160* const nullObj = nullptr;
        ss << size_t(static_cast<void*>(&(nullObj->_hash)));
        ss << typeid(nullObj->_hash).name();

        dtds.add_decoded_type_data_to_map(std::move(decoded_type_data(calculate_checksum_from_string(ss.str()), boost::core::demangle(type_id.c_str()), sizeof(fc::ripemd160), alignof(fc::ripemd160))));
      }
    };

    template<typename... Args>
    struct specific_type_decoder<fc::static_variant<Args...>>
    {
      void decode(decoded_types_data_storage& dtds) const
      {
        std::stringstream ss;
        const std::string type_id = typeid(fc::static_variant<Args...>).name();
        ss << type_id;
        ss << sizeof(fc::static_variant<Args...>);
        ss << alignof(fc::static_variant<Args...>);

        typedef boost::mpl::vector<Args...> types_vector;

        if (boost::mpl::size<types_vector>::value)
        {
          fc::static_variant<Args...> sv;
          ss << typeid(sv.which()).name();
          type_analyzer analyzer(dtds);
          boost::mpl::for_each<typename boost::mpl::transform<types_vector, boost::add_pointer<boost::mpl::_1>>::type>(analyzer);
        }

        dtds.add_decoded_type_data_to_map(std::move(decoded_type_data(calculate_checksum_from_string(ss.str()), boost::core::demangle(type_id.c_str()), sizeof(fc::static_variant<Args...>), alignof(fc::static_variant<Args...>))));

        serialized_type_data::entries_vector_t alternatives;
        ( alternatives.push_back({.name = std::to_string(alternatives.size()), .value = canonical_type_name<Args>()}), ... );
        dtds.add_serialized_type_data_to_map(serialized_type_data{.name = canonical_type_name<fc::static_variant<Args...>>(), .kind = "static_variant", .entries = std::move(alternatives)});
      }
    };

    template<>
    struct specific_type_decoder<fc::sha256>
    {
      void decode(decoded_types_data_storage& dtds) const
      {
        std::stringstream ss;
        const std::string type_id = typeid(fc::sha256).name();
        ss << type_id;
        ss << sizeof(fc::sha256);
        ss << alignof(fc::sha256);

        fc::sha256* const nullObj = nullptr;
        ss << size_t(static_cast<void*>(&(nullObj->_hash)));
        ss << typeid(nullObj->_hash).name();

        dtds.add_decoded_type_data_to_map(std::move(decoded_type_data(calculate_checksum_from_string(ss.str()), boost::core::demangle(type_id.c_str()), sizeof(fc::sha256), alignof(fc::sha256))));
      }
    };

    template<typename T, size_t N>
    struct specific_type_decoder<fc::int_array<T, N>>
    {
      void decode(decoded_types_data_storage& dtds) const
      {
        std::stringstream ss;
        const std::string type_id = typeid(fc::int_array<T, N>).name();
        ss << type_id;
        ss << sizeof(fc::int_array<T, N>);
        ss << alignof(fc::int_array<T, N>);

        fc::int_array<T, N>* const nullObj = nullptr;
        ss << size_t(static_cast<void*>(&(nullObj->data)));
        ss << typeid(nullObj->data).name();

        dtds.add_decoded_type_data_to_map(std::move(decoded_type_data(calculate_checksum_from_string(ss.str()), boost::core::demangle(type_id.c_str()), sizeof(fc::int_array<T, N>), alignof(fc::int_array<T, N>))));
        main_decoder<T> decoder;
        decoder.decode(dtds);
      }
    };

    template<typename A, typename B>
    struct specific_type_decoder<std::pair<A, B>>
    {
      void decode(decoded_types_data_storage& dtds) const
      {
        main_decoder<A> decoder_A;
        decoder_A.decode(dtds);
        main_decoder<B> decoder_B;
        decoder_B.decode(dtds);
      }
    };

    template<typename T>
    struct specific_type_decoder<std::optional<T>>
    {
      void decode(decoded_types_data_storage& dtds) const
      {
        main_decoder<T> decoder;
        decoder.decode(dtds);
      }
    };

    /* End of specific decoders for non reflected types. */

  } // hive::chain::util::decoders::non_reflected_types
} // hive::chain::util::decoders

} } } // hive::chain::util

FC_REFLECT(hive::chain::util::decoded_type_data::member_data, (type)(name)(offset))
FC_REFLECT(hive::chain::util::decoded_type_data, (name)(size_of)(align_of)(checksum)(reflected)(members)(enum_values))
FC_REFLECT(hive::chain::util::serialized_type_data::entry, (name)(value))
FC_REFLECT(hive::chain::util::serialized_type_data, (name)(kind)(entries))
