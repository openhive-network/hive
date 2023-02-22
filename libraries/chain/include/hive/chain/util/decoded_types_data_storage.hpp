#pragma once

#include <chainbase/allocators.hpp>

#include <fc/crypto/ripemd160.hpp>
#include <fc/exception/exception.hpp>
#include <fc/reflect/reflect.hpp>

#include <unordered_map>
#include <unordered_set>

#include <boost/mpl/vector.hpp>
#include <boost/mpl/for_each.hpp>

namespace hive { namespace chain { namespace util {

fc::ripemd160 calculate_checksum_from_string(const std::string_view type_id);

class decoded_type_data
{
  public:
    decoded_type_data(const fc::ripemd160& _checksum, const std::string_view _type_id, const bool _reflected);

    virtual fc::ripemd160 get_checksum() const final { return checksum; }
    virtual std::string_view get_type_id() const final { return type_id; }
    virtual bool is_reflected() const final { return reflected; }

  protected:
    const std::string checksum;
    const std::string type_id;
    const bool reflected;
};

class reflected_decoded_type_data : public decoded_type_data
{
  public:
    using members_vector = std::vector<std::pair<std::string, std::string>>;
    using enum_values_vector = std::vector<std::pair<std::string, size_t>>;

    reflected_decoded_type_data(const fc::ripemd160& _checksum, const std::string_view _type_id, const std::string_view _fc_name,
                                members_vector&& _members, enum_values_vector&& _enum_values);

    std::string_view get_type_name() const { return fc_name; }
    bool is_enum() const { return members.empty(); }
    const members_vector& get_members() const { return members; }
    const enum_values_vector& get_enum_values() const { return enum_values; }

  private:
    const members_vector members; // in case of structure
    const enum_values_vector enum_values; // in case of enum
    const std::string fc_name;
};

class decoded_types_data_storage final
{
  private:
    using decoding_types_set_t = std::unordered_set<std::string_view>;
    using decoded_types_map_t = std::unordered_map<std::string_view, std::shared_ptr<decoded_type_data>>;

    /* Private methods */

    decoded_types_data_storage();
    ~decoded_types_data_storage();

    bool add_type_to_decoded_types_set(const std::string_view type_name);
    void add_decoded_type_data_to_map(const std::shared_ptr<decoded_type_data>& decoded_type);

    /* Main decoder - distinguish which type is reflected or not. */
    template<typename T, bool is_defined = fc::reflector<T>::is_defined::value>
    class decoder;

    /* Detector for analysing template arguments. */
    template <typename>
    struct template_types_detector : std::false_type { void analyze_arguments() const {} };

    template <template <typename...> typename T, typename... Args>
    struct template_types_detector<T<Args...>> : std::bool_constant<sizeof...(Args) != 0U>
    {
      private:
        struct types_vector_analyzer
        {
          template<typename U> void operator()(U*) const
          {
            decoder<U> decoder;
            decoder.decode();
          }
        };

      public:
        void analyze_arguments() const
        {
          typedef boost::mpl::vector<Args...> types_vector;
          boost::mpl::for_each<typename boost::mpl::transform<types_vector, boost::add_pointer<boost::mpl::_1>>::type>(types_vector_analyzer());
        }
    };

    /* Tools for decoding reflected types and enums. */
    struct visitor_defined_types_detector
    {
      template <typename Member, class Class, Member(Class::*member)>
      void operator()(const char *name) const
      {
        if (fc::reflector<Member>::is_defined::value)
        {
          decoder<Member> decoder;
          decoder.decode();
        }

        if (template_types_detector<Member>::value)
        {
          template_types_detector<Member> detector;
          detector.analyze_arguments();
        }
      }
    };

    class visitor_type_decoder
    {
    public:
      visitor_type_decoder(reflected_decoded_type_data::members_vector& _members, const std::string_view type_name) : members(_members)
      {
        encoder.write(type_name.data(), type_name.size());
      }

      template <typename Member, class Class, Member(Class::*member)>
      void operator()(const char *name) const
      {
        const std::string field_name(name);
        const std::string type_id(typeid(Member).name());
        Class* const nullObj = nullptr;
        const std::string member_offset = std::to_string(size_t(static_cast<void*>(&(nullObj->*member))));

        encoder.write(type_id.data(), type_id.size());
        encoder.write(field_name.data(), field_name.size());
        encoder.write(member_offset.data(), member_offset.size());
        members.push_back({fc::get_typename<Member>::name(), field_name});
      }
      
      fc::ripemd160 get_checksum() { return encoder.result(); }

    private:
      mutable fc::ripemd160::encoder encoder;
      reflected_decoded_type_data::members_vector& members;
    };

    class visitor_enum_decoder
    {
    public:
      visitor_enum_decoder(reflected_decoded_type_data::enum_values_vector& _enum_values, const std::string_view enum_name) : enum_values(_enum_values)
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
      reflected_decoded_type_data::enum_values_vector& enum_values;
    };

    /* Allows to check if type is specified template type without parsing template arguments. */
    template<typename U, template<typename...> class Ref>
    struct is_specialization : std::false_type {};

    template<template<typename...> class Ref, typename... Args>
    struct is_specialization<Ref<Args...>, Ref>: std::true_type {};

    /* Decoding starting point. */
    template<typename T>
    struct decoder<T, true /* is_defined - type is reflected */>
    {
      void decode() const
      {
        if constexpr (fc::reflector<T>::is_enum::value)
        {
          const std::string_view enum_id_name = typeid(T).name();

          if (!get_instance().add_type_to_decoded_types_set(enum_id_name))
            return;

          reflected_decoded_type_data::enum_values_vector enum_values;
          visitor_enum_decoder visitor(enum_values, enum_id_name);
          fc::reflector<T>::visit(visitor);
          const std::string_view type_name = fc::get_typename<T>::name();
          get_instance().add_decoded_type_data_to_map(std::make_shared<reflected_decoded_type_data>(visitor.get_checksum(), enum_id_name, type_name, std::move(reflected_decoded_type_data::members_vector()), std::move(enum_values)));
        }
        else
        {
          const std::string_view type_id_name = typeid(T).name();

          if (!get_instance().add_type_to_decoded_types_set(type_id_name))
            return;

          fc::reflector<T>::visit(visitor_defined_types_detector());
          reflected_decoded_type_data::members_vector members;
          visitor_type_decoder visitor(members, type_id_name);
          fc::reflector<T>::visit(visitor);
          const std::string_view type_name = fc::get_typename<T>::name();
          get_instance().add_decoded_type_data_to_map(std::make_shared<reflected_decoded_type_data>(visitor.get_checksum(), type_id_name, type_name, std::move(members), std::move(reflected_decoded_type_data::enum_values_vector())));
        }
      }
    };

    template<typename T>
    struct decoder<T, false /* is_defined - type is not reflected */>
    {
      void decode() const
      {
        /* Type is fundamental - do nothing. */
        if constexpr (std::is_fundamental<T>::value) {}
        /* Type is trival. */
        else if constexpr (std::is_trivial<T>::value)
        {
          const std::string_view type_id = typeid(T).name();
          if (template_types_detector<T>::value &&
              get_instance().add_type_to_decoded_types_set(type_id))
          {
            template_types_detector<T> detector;
            detector.analyze_arguments();
          }
        }
        /* Type is an index. */
        else if constexpr (is_specialization<T, boost::multi_index::multi_index_container>::value)
        {
          const std::string_view index_type_id = typeid(T).name();

          if (get_instance().add_type_to_decoded_types_set(index_type_id))
          {
            get_instance().add_decoded_type_data_to_map(std::make_shared<decoded_type_data>(calculate_checksum_from_string(index_type_id), index_type_id, false));
            decoder<typename T::value_type> decoder;
            decoder.decode();
          }
        }
        /* Type is an allocator. */
        else if constexpr (is_specialization<T, boost::interprocess::allocator>::value)
        {
          const std::string_view allocator_type_id = typeid(T).name();
          if (get_instance().add_type_to_decoded_types_set(allocator_type_id))
            get_instance().add_decoded_type_data_to_map(std::make_shared<decoded_type_data>(calculate_checksum_from_string(allocator_type_id), allocator_type_id, false));
        }
        /* Rest cases */
        else {}
      }
    };

  public:
    decoded_types_data_storage(const decoded_types_data_storage&) = delete;
    decoded_types_data_storage& operator=(const decoded_types_data_storage&) = delete;
    decoded_types_data_storage(decoded_types_data_storage&&) = delete;
    decoded_types_data_storage& operator=(decoded_types_data_storage&&) = delete;
    
    static decoded_types_data_storage& get_instance();

    template <typename T>
    void register_new_type()
    {
      decoder<T> decoder;
      decoder.decode();
    }

    template <typename T>
    std::shared_ptr<decoded_type_data> get_decoded_type_data()
    {
      register_new_type<T>();
      auto it = decoded_types_data_map.find(typeid(T).name());

      if (it == decoded_types_data_map.end())
        FC_THROW_EXCEPTION( fc::key_not_found_exception, "Cound not find decoded type even after decoding it. Type id: ${name} .", ("name", std::string(typeid(T).name())) );
      
      return it->second;
    }

    template <typename T>
    fc::ripemd160 get_decoded_type_checksum()
    {
      return get_decoded_type_data<T>()->get_checksum();
    }

    const std::unordered_map<std::string_view, std::shared_ptr<decoded_type_data>>& get_decoded_types_data_map() const { return decoded_types_data_map; }

  private:
    std::unordered_set<std::string_view> decoded_types_set; // we store typeid(T).name() here
    std::unordered_map<std::string_view, std::shared_ptr<decoded_type_data>> decoded_types_data_map; //key - type_id
};

} } } // hive::chain::util