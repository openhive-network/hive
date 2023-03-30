#pragma once

#include <chainbase/allocators.hpp>
#include <chainbase/chainbase.hpp>

#include <fc/crypto/ripemd160.hpp>
#include <fc/crypto/sha256.hpp>
#include <fc/exception/exception.hpp>
#include <fc/io/json.hpp>
#include <fc/reflect/reflect.hpp>
#include <fc/static_variant.hpp>

#include <hive/protocol/fixed_string.hpp>

#include <unordered_map>
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
  decoded_type_data(const std::string_view _checksum, const std::string_view _type_id);
  decoded_type_data(const std::string_view _checksum, const std::string_view _type_id, const std::string_view _name,
                    members_vector_t&& _members, enum_values_vector_t&& _enum_values);

  // json pattern: "{"reflected": bool, "type_id": string, "checksum": string, "size_of": int, "align_of": int}"
  // json pattern: "{"reflected": bool, "type_id": string, "checksum": string, "name": string, "size_of": int, "align_of": int, "members": [{"type":string, "name":string, "offset":int}, ...}]}"
  // json pattern: "{"reflected": bool, "type_id": string, "checksum": string, "name": string, "enum_values": [["enum",int], ...]}"
  decoded_type_data(const std::string& json);

  fc::optional<std::string> name;
  fc::optional<members_vector_t> members; // in case of structure
  fc::optional<enum_values_vector_t> enum_values; // in case of enum
  std::string type_id;
  std::string checksum;
  bool reflected;
};

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

class decoded_types_data_storage final
{
  private:
    bool add_type_to_decoded_types_set(const std::string_view type_name);
    void add_decoded_type_data_to_map(decoded_type_data&& decoded_type);

    template <typename T> friend class decoders::main_decoder;
    template <typename ...Args> friend class decoders::non_reflected_types::specific_type_decoder;

  public:
    using decoding_types_set_t = std::unordered_set<std::string>;
    using decoded_types_map_t = std::unordered_map<std::string, decoded_type_data>;

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
      auto it = decoded_types_data_map.find(typeid(T).name());

      if (it == decoded_types_data_map.end())
        FC_THROW_EXCEPTION( fc::key_not_found_exception, "Cound not find decoded type data. Type id: ${name} .", ("name", std::string(typeid(T).name())) );

      return it->second;
    }

    template <typename T>
    std::string_view get_decoded_type_checksum() const
    {
      return get_decoded_type_data<T>().checksum;
    }

    const decoded_types_map_t& get_decoded_types_data_map() const { return decoded_types_data_map; }

    // if json string is specified, generate pretty string with info from it, otherwise generate output from map with current decoded types data.
    std::string generate_decoded_types_data_pretty_string(const std::string& decoded_types_data_json = std::string()) const;
    std::string generate_decoded_types_data_json_string() const;

    // bool - comparing result, string - details of detected differences.
    std::pair<bool, std::string> check_if_decoded_types_data_json_matches_with_current_decoded_data(const std::string& decoded_types_data_json) const;

  private:
    decoding_types_set_t decoded_types_set; // we store typeid(T).name() here
    decoded_types_map_t decoded_types_data_map; //key - type_id
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

  template <template <typename...> typename T, typename... Args>
  struct template_types_detector<T<Args...>> : std::bool_constant<sizeof...(Args) != 0U>
  {
    private:
      decoded_types_data_storage& dtds;

      struct types_vector_analyzer
      {
        types_vector_analyzer(decoded_types_data_storage& _dtds) : dtds(_dtds) {}

        template<typename U> void operator()(U*)
        {
          main_decoder<U> decoder;
          decoder.decode(dtds);
        }

        private:
          decoded_types_data_storage& dtds;
      };

    public:
      template_types_detector(decoded_types_data_storage& _dtds) : dtds(_dtds) {}

      void analyze_arguments()
      {
        typedef boost::mpl::vector<Args...> types_vector;
        types_vector_analyzer analyzer(dtds);
        boost::mpl::for_each<typename boost::mpl::transform<types_vector, boost::add_pointer<boost::mpl::_1>>::type>(analyzer);
      }
  };

  /* Tools for decoding reflected types and enums. */
  class visitor_type_decoder
  {
    public:
      visitor_type_decoder(decoded_type_data::members_vector_t& _members, const std::string_view type_name, decoded_types_data_storage& _dtds) : members(_members), dtds(_dtds)
      {
        encoder.write(type_name.data(), type_name.size());
      }

      template <typename Member, class Class, Member(Class::*member)>
      void operator()(const char *name) const
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
        members.push_back({.type = fc::get_typename<Member>::name(), .name = field_name, .offset = member_offset});
      }
      
      fc::ripemd160 get_checksum() { return encoder.result(); }

    private:
      mutable fc::ripemd160::encoder encoder;
      decoded_type_data::members_vector_t& members;
      decoded_types_data_storage& dtds;
    };

  class visitor_enum_decoder
  {
    public:
      visitor_enum_decoder(decoded_type_data::enum_values_vector_t& _enum_values, const std::string_view enum_name) : enum_values(_enum_values)
      {
        encoder.write(enum_name.data(), enum_name.size());
      }

      void operator()(const char *name, const int64_t value) const
      {
        const std::string value_name(name);
        const std::string value_in_string(std::to_string(value));
        encoder.write(value_name.data(), value_name.size());
        encoder.write(value_in_string.data(), value_in_string.size());
        enum_values.push_back({value_name, value});
      }

      fc::ripemd160 get_checksum() { return encoder.result(); }

    private:
      mutable fc::ripemd160::encoder encoder;
      decoded_type_data::enum_values_vector_t& enum_values;
    };

  /* Throws a compilation error in case of instantiation. */
  template <typename T> struct unable_to_decode_type_error;

  /* Allows to check if type is specified template type without parsing template arguments. */
  template<typename U, template<typename...> class Ref>
  struct is_specialization : std::false_type {};

  template<template<typename...> class Ref, typename... Args>
  struct is_specialization<Ref<Args...>, Ref> : std::true_type {};

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
        decoded_type_data::enum_values_vector_t enum_values;
        visitor_enum_decoder visitor(enum_values, type_id_name);
        fc::reflector<T>::visit(visitor);
        const std::string_view type_name = fc::get_typename<T>::name();
        dtds.add_decoded_type_data_to_map(std::move(
          decoded_type_data(visitor.get_checksum().str(), type_id_name, type_name, std::move(decoded_type_data::members_vector_t()), std::move(enum_values))));
      }
      else
      {
        decoded_type_data::members_vector_t members;
        visitor_type_decoder visitor(members, type_id_name, dtds);
        fc::reflector<T>::visit(visitor);
        const std::string_view type_name = fc::get_typename<T>::name();
        dtds.add_decoded_type_data_to_map(std::move(
          decoded_type_data(visitor.get_checksum().str(), type_id_name, type_name, std::move(members), std::move(decoded_type_data::enum_values_vector_t()))));
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
        const std::string_view index_type_id = typeid(T).name();

        if (dtds.add_type_to_decoded_types_set(index_type_id))
        {
          dtds.add_decoded_type_data_to_map(std::move(decoded_type_data(calculate_checksum_from_string(index_type_id), index_type_id)));
          main_decoder<typename T::value_type> decoder;
          decoder.decode(dtds);
        }
      }
      /* Boost types. We control boost types via checking boost version, so only check if typeid of these types match (it will change in case of any template parameters change). */
      else if constexpr (is_specialization<T, boost::interprocess::allocator>::value ||
                         is_specialization<T, boost::container::basic_string>::value ||
                         is_specialization<T, boost::container::flat_map>::value ||
                         is_specialization<T, boost::container::flat_set>::value ||
                         is_specialization<T, boost::container::deque>::value ||
                         is_specialization<T, boost::container::vector>::value)
      {
        const std::string_view boost_type_id = typeid(T).name();

        if (dtds.add_type_to_decoded_types_set(boost_type_id))
          dtds.add_decoded_type_data_to_map(std::move(decoded_type_data(calculate_checksum_from_string(boost_type_id), boost_type_id)));
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
        const std::string_view type_id = typeid(chainbase::oid<T>).name();
        ss << type_id;
        ss << sizeof(chainbase::oid<T>);
        ss << alignof(chainbase::oid<T>);

        chainbase::oid<T> oid(0);
        using oid_value_type = decltype(oid.get_value());
        ss << sizeof(oid_value_type);
        ss << alignof(oid_value_type);
        ss << typeid(oid_value_type).name();

        dtds.add_decoded_type_data_to_map(std::move(decoded_type_data(calculate_checksum_from_string(ss.str()), type_id)));
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
        const std::string_view type_id = typeid(chainbase::oid_ref<T>).name();
        ss << type_id;
        ss << sizeof(chainbase::oid_ref<T>);
        ss << alignof(chainbase::oid_ref<T>);

        chainbase::oid_ref<T> oid_ref;
        using oid_value_type = decltype(oid_ref.get_value());
        ss << sizeof(oid_value_type);
        ss << alignof(oid_value_type);
        ss << typeid(oid_value_type).name();

        dtds.add_decoded_type_data_to_map(std::move(decoded_type_data(calculate_checksum_from_string(ss.str()), type_id)));
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
        const std::string_view type_id = typeid(fc::time_point_sec).name();
        ss << type_id;
        ss << sizeof(fc::time_point_sec);
        ss << alignof(fc::time_point_sec);

        fc::time_point_sec tps;
        using sec_since_epoch_type = decltype(tps.sec_since_epoch());
        ss << sizeof(sec_since_epoch_type);
        ss << alignof(sec_since_epoch_type);
        ss << typeid(sec_since_epoch_type).name();

        dtds.add_decoded_type_data_to_map(std::move(decoded_type_data(calculate_checksum_from_string(ss.str()), type_id)));
      }
    };

    template<typename A, typename B>
    struct specific_type_decoder<fc::erpair<A, B>>
    {
      void decode(decoded_types_data_storage& dtds) const
      {
        std::stringstream ss;
        const std::string_view type_id = typeid(fc::erpair<A, B>).name();
        ss << type_id;
        ss << sizeof(fc::erpair<A, B>);
        ss << alignof(fc::erpair<A, B>);

        fc::erpair<A, B>* const nullObj = nullptr;
        ss << size_t(static_cast<void*>(&(nullObj->first)));
        ss << typeid(nullObj->first).name();
        ss << size_t(static_cast<void*>(&(nullObj->second)));
        ss << typeid(nullObj->second).name();

        dtds.add_decoded_type_data_to_map(std::move(decoded_type_data(calculate_checksum_from_string(ss.str()), type_id)));

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
        const std::string_view type_id = typeid(fc::array<T, N>).name();
        ss << type_id;
        ss << sizeof(fc::array<T, N>);
        ss << alignof(fc::array<T, N>);

        fc::array<T, N>* const nullObj = nullptr;
        ss << size_t(static_cast<void*>(&(nullObj->data)));
        ss << typeid(nullObj->data).name();

        dtds.add_decoded_type_data_to_map(std::move(decoded_type_data(calculate_checksum_from_string(ss.str()), type_id)));
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
        const std::string_view type_id = typeid(hive::protocol::fixed_string_impl<T>).name();
        ss << type_id;
        ss << sizeof(hive::protocol::fixed_string_impl<T>);
        ss << alignof(hive::protocol::fixed_string_impl<T>);

        hive::protocol::fixed_string_impl<T>* const nullObj = nullptr;
        ss << size_t(static_cast<void*>(&(nullObj->data)));
        ss << typeid(nullObj->data).name();

        dtds.add_decoded_type_data_to_map(std::move(decoded_type_data(calculate_checksum_from_string(ss.str()), type_id)));
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
        const std::string_view type_id = typeid(fc::ripemd160).name();
        ss << type_id;
        ss << sizeof(fc::ripemd160);
        ss << alignof(fc::ripemd160);

        fc::ripemd160* const nullObj = nullptr;
        ss << size_t(static_cast<void*>(&(nullObj->_hash)));
        ss << typeid(nullObj->_hash).name();

        dtds.add_decoded_type_data_to_map(std::move(decoded_type_data(calculate_checksum_from_string(ss.str()), type_id)));
      }
    };

    template<typename T>
    struct specific_type_decoder<fc::static_variant<T>>
    {
      void decode(decoded_types_data_storage& dtds) const
      {
        std::stringstream ss;
        const std::string_view type_id = typeid(fc::static_variant<T>).name();
        ss << type_id;
        ss << sizeof(fc::static_variant<T>);
        ss << alignof(fc::static_variant<T>);

        fc::static_variant<T> sv;
        ss << typeid(sv.which()).name();

        dtds.add_decoded_type_data_to_map(std::move(decoded_type_data(calculate_checksum_from_string(ss.str()), type_id)));
        main_decoder<T> decoder;
        decoder.decode(dtds);
      }
    };

    template<>
    struct specific_type_decoder<fc::static_variant<>>
    {
      void decode(decoded_types_data_storage& dtds) const
      {
        std::stringstream ss;
        const std::string_view type_id = typeid(fc::static_variant<>).name();
        ss << type_id;
        ss << sizeof(fc::static_variant<>);
        ss << alignof(fc::static_variant<>);
        dtds.add_decoded_type_data_to_map(std::move(decoded_type_data(calculate_checksum_from_string(ss.str()), type_id)));
      }
    };

    template<>
    struct specific_type_decoder<fc::sha256>
    {
      void decode(decoded_types_data_storage& dtds) const
      {
        std::stringstream ss;
        const std::string_view type_id = typeid(fc::sha256).name();
        ss << type_id;
        ss << sizeof(fc::sha256);
        ss << alignof(fc::sha256);

        fc::sha256* const nullObj = nullptr;
        ss << size_t(static_cast<void*>(&(nullObj->_hash)));
        ss << typeid(nullObj->_hash).name();

        dtds.add_decoded_type_data_to_map(std::move(decoded_type_data(calculate_checksum_from_string(ss.str()), type_id)));
      }
    };

    template<typename T, size_t N>
    struct specific_type_decoder<fc::int_array<T, N>>
    {
      void decode(decoded_types_data_storage& dtds) const
      {
        std::stringstream ss;
        const std::string_view type_id = typeid(fc::int_array<T, N>).name();
        ss << type_id;
        ss << sizeof(fc::int_array<T, N>);
        ss << alignof(fc::int_array<T, N>);

        fc::int_array<T, N>* const nullObj = nullptr;
        ss << size_t(static_cast<void*>(&(nullObj->data)));
        ss << typeid(nullObj->data).name();

        dtds.add_decoded_type_data_to_map(std::move(decoded_type_data(calculate_checksum_from_string(ss.str()), type_id)));
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

    /* End of specific decoders for non reflected types. */

  } // hive::chain::util::decoders::non_reflected_types
} // hive::chain::util::decoders

} } } // hive::chain::util

FC_REFLECT(hive::chain::util::decoded_type_data::member_data, (type)(name)(offset))
FC_REFLECT(hive::chain::util::decoded_type_data, (name)(members)(enum_values)(type_id)(checksum)(reflected))
