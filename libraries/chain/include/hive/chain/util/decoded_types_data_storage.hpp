#pragma once

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

    decoded_type_data(const size_t _checksum, const std::string_view _name, members_vector&& _members, enum_values_vector _enum_values);

    size_t get_checksum() const { return checksum; }
    std::string_view get_type_name() const { return name; }
    bool is_enum() const { return members.empty(); }

  private:
    size_t checksum = 0;
    std::string_view name;
    members_vector members; // in case of structure
    enum_values_vector enum_values; // in case of enum
};

class decoded_types_data_storage final
{
  private:
    using decoding_types_set_t = std::unordered_set<std::string_view>;
    using decoded_types_map_t = std::unordered_map<std::string_view, decoded_type_data>;

    /* Private methods & class definitions */

    decoded_types_data_storage();
    ~decoded_types_data_storage();

    void add_type_decoding_set(const std::string_view type_name);
    void remove_type_decoding_set(const std::string_view type_name);
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
          get_instance().add_type_decoding_set(type);

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

          get_instance().remove_type_decoding_set(type);
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
        checksum += pos + sizeof(Member);
        ++pos;
        members.push_back({fc::get_typename<Member>::name(), name});
      }
      
      size_t get_checksum() { return checksum; }

    private:
      decoded_type_data::members_vector& members;
      mutable size_t checksum = 0;
      mutable size_t pos = 0;
    };

    class visitor_enum_decoder
    {
    public:
      visitor_enum_decoder(decoded_type_data::enum_values_vector& _enum_values) : enum_values(_enum_values) {}

      void operator()(const char *name, const int64_t value) const
      {
        checksum += std::hash<std::string>{}(name) + value;
        enum_values.push_back({name, value});
      }

      size_t get_checksum() { return checksum; }

    private:
      decoded_type_data::enum_values_vector& enum_values;
      mutable size_t checksum = 0;
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
    size_t get_decoded_type_checksum()
    {
      return get_decoded_type_data<T>().get_checksum();
    }

  private:
    std::unordered_set<std::string_view> types_being_decoded_set;
    std::unordered_map<std::string_view, decoded_type_data> decoded_types_data_map;
    std::shared_mutex mutex;
};

} } } // hive::chain::util