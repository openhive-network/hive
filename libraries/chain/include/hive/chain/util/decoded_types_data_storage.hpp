#pragma once

#include <fc/crypto/ripemd160.hpp>
#include <fc/exception/exception.hpp>
#include <fc/reflect/reflect.hpp>

#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>

namespace hive { namespace chain { namespace util {

class decoded_type_data
{
  public:
    using members_vector = std::vector<std::pair<std::string, std::string>>;
    using enum_values_vector = std::vector<std::pair<std::string, size_t>>;

    decoded_type_data(const fc::ripemd160& _checksum, const std::string_view _name, members_vector&& _members, enum_values_vector _enum_values);

    fc::ripemd160 get_checksum() const { return checksum; }
    std::string_view get_type_name() const { return name; }
    bool is_enum() const { return members.empty(); }
    const members_vector& get_members() const { return members; }
    const enum_values_vector& get_enum_values() const { return enum_values; }

  private:
    members_vector members; // in case of structure
    enum_values_vector enum_values; // in case of enum
    fc::ripemd160 checksum;
    std::string_view name;
};

class decoded_types_data_storage final
{
  private:
    using decoding_types_set_t = std::unordered_set<std::string_view>;
    using decoded_types_map_t = std::unordered_map<std::string_view, decoded_type_data>;

    /* Private methods & class definitions */

    decoded_types_data_storage();
    ~decoded_types_data_storage();

    void add_type_to_decoding_set(const std::string_view type_name);
    void remove_type_from_decoding_set(const std::string_view type_name);
    void add_decoded_type_data_to_map(decoded_type_data&& decoded_type);
    bool type_is_being_decoded_or_already_decoded(const std::string_view type_name);

    template<typename T, bool is_defined = fc::reflector<T>::is_defined::value, bool is_enum = fc::reflector<T>::is_enum::value>
    class decoder;

    template<typename T>
    struct decoder<T, false /* is_defined */, false /* is_enum */>
    {
      void decode()
      {
      }
    };

    template<typename T>
    struct decoder<T, false /* is_defined */, true /* is_enum */>
    {
      void decode()
      {
      }
    };

    struct visitor_defined_types_detector
    {
      template <typename Member, class Class, Member(Class::*member)>
      void operator()(const char *name) const
      {
        if (!fc::reflector<Member>::is_defined::value)
          return;

        std::string_view type = fc::get_typename<Member>::name();

        if (!get_instance().type_is_being_decoded_or_already_decoded(type))
        {
          get_instance().add_type_to_decoding_set(type);

          if (fc::reflector<Member>::is_enum::value)
          {
            decoder<Member> decoder;
            decoder.decode();
          }
          else
          {
            decoder<Member> decoder;
            decoder.decode();
          }

          get_instance().remove_type_from_decoding_set(type);
        }
      }
    };

    class visitor_type_decoder
    {
    public:
      visitor_type_decoder(decoded_type_data::members_vector& _members) : members(_members) {}

      template <typename Member, class Class, Member(Class::*member)>
      void operator()(const char *name) const
      {
        const std::string field_name(name);
        const std::string type_id(typeid(Member).name());

        encoder.write(type_id.data(), type_id.size());
        encoder.write(field_name.data(), field_name.size());
        members.push_back({fc::get_typename<Member>::name(), field_name});
      }
      
      fc::ripemd160 get_checksum() { return encoder.result(); }

    private:
      mutable fc::ripemd160::encoder encoder;
      decoded_type_data::members_vector& members;
    };

    class visitor_enum_decoder
    {
    public:
      visitor_enum_decoder(decoded_type_data::enum_values_vector& _enum_values) : enum_values(_enum_values) {}

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
      decoded_type_data::enum_values_vector& enum_values;
  };

    template<typename T>
    struct decoder<T, true /* is_defined */, false /* is_enum */>
    {
      void decode()
      {
        fc::reflector<T>::visit(visitor_defined_types_detector());
        decoded_type_data::members_vector members;
        visitor_type_decoder visitor(members);
        fc::reflector<T>::visit(visitor);
        const std::string_view type_name = fc::get_typename<T>::name();
        get_instance().add_decoded_type_data_to_map(std::move(decoded_type_data(visitor.get_checksum(), type_name, std::move(members), std::move(decoded_type_data::enum_values_vector()))));
      }
    };

    template<typename T>
    struct decoder<T, true /* is_defined */, true /* is_enum */>
    {
      void decode()
      {
        decoded_type_data::enum_values_vector enum_values;
        visitor_enum_decoder visitor(enum_values);
        fc::reflector<T>::visit(visitor);
        const std::string_view type_name = fc::get_typename<T>::name();
        get_instance().add_decoded_type_data_to_map(std::move(decoded_type_data(visitor.get_checksum(), type_name, std::move(decoded_type_data::members_vector()), std::move(enum_values))));
      }
    }; 

  public:
    decoded_types_data_storage(const decoded_types_data_storage&) = delete;
    decoded_types_data_storage& operator=(const decoded_types_data_storage&) = delete;
    decoded_types_data_storage(decoded_types_data_storage&&) = delete;
    decoded_types_data_storage& operator=(decoded_types_data_storage&&) = delete;
    
    static decoded_types_data_storage& get_instance();

    template <typename T, bool is_defined = fc::reflector<T>::is_defined::value>
    void register_new_type()
    {
      static_assert(is_defined);
      if (type_is_being_decoded_or_already_decoded(fc::get_typename<T>::name()))
        return;

      decoder<T> decoder;
      decoder.decode();
    }

    template <typename T, bool is_defined = fc::reflector<T>::is_defined::value>
    decoded_type_data get_decoded_type_data()
    {
      static_assert(is_defined);
      register_new_type<T>();
      std::shared_lock<std::shared_mutex> read_lock(mutex);
      auto it = decoded_types_data_map.find(fc::get_typename<T>::name());

      if (it == decoded_types_data_map.end())
        FC_THROW_EXCEPTION( fc::key_not_found_exception, "Cound not find decoded type even after decoding it. Type name: ${name} .", ("name", std::string(fc::get_typename<T>::name())) );
      
      return it->second;
    }

    template <typename T>
    fc::ripemd160 get_decoded_type_checksum()
    {
      return get_decoded_type_data<T>().get_checksum();
    }

    const std::unordered_map<std::string_view, decoded_type_data>& get_decoded_types_data_map() const { return decoded_types_data_map; }

  private:
    std::unordered_set<std::string_view> types_being_decoded_set;
    std::unordered_map<std::string_view, decoded_type_data> decoded_types_data_map;
    std::shared_mutex mutex;
};

} } } // hive::chain::util