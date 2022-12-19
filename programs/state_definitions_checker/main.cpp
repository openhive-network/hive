#include <hive/chain/account_object.hpp>

#include <iostream>
#include <sstream>
#include <unordered_map>

struct member_data
{
  std::string type_or_value;
  std::string name;
};

class decoded_type
{
  public:
    decoded_type(const size_t _checksum, const std::string_view _name, std::vector<member_data>&& _members, const bool _is_enum) :
      checksum(_checksum), name(_name), members(std::move(_members)), is_enum(_is_enum) {}
    void print_type_info() const;

  private:
    size_t checksum = 0;
    std::string_view name;
    std::vector<member_data> members;
    bool is_enum;
};

void decoded_type::print_type_info() const
{
  std::stringstream ss;
  ss << "Type: " << name << "\n";
  ss << "Checksum: " << checksum << "\n";

  if (is_enum)
    ss << "Values:" << "\n";
  else
    ss << "Members:" << "\n";

  for (const auto& member : members)
    ss << "  " << member.name << ": " << member.type_or_value << "\n";

  ss << "\n";

  std::cerr << ss.str();
}

std::unordered_set<std::string_view> types_being_decoded;
std::unordered_map<std::string_view, decoded_type> decoded_types;

template<typename T, bool type_is_defined = fc::reflector<T>::is_defined::value, bool type_is_enum = fc::reflector<T>::is_enum::value>
class type_decoder;

template<typename T>
class type_decoder<T,
                   fc::reflector<T>::is_enum::value ? true : false,
                   fc::reflector<T>::is_enum::value>
{
  public:    
    void decode() const {}
};

template<typename T, bool type_is_defined = fc::reflector<T>::is_defined::value, bool type_is_enum = fc::reflector<T>::is_enum::value>
class enum_decoder;

template<typename T>
class enum_decoder<T, false, fc::reflector<T>::is_enum::value>
{
  public:
    void decode() const {}
};

template<typename T>
class enum_decoder<T, true, false>
{
  public:
    void decode() const {}
};

namespace visitors
{
  struct defined_types_detector
  {
    template <typename Member, class Class, Member(Class::*member)>
    void operator()(const char *name) const
    {
      std::string_view type = fc::get_typename<Member>::name();

      if (fc::reflector<Member>::is_defined::value &&
          decoded_types.find(type) == decoded_types.end() &&
          types_being_decoded.find(type) == types_being_decoded.end())
      {
        types_being_decoded.emplace(type);

        if (fc::reflector<Member>::is_enum::value)
        {
          enum_decoder<Member> decoder;
          decoder.decode();
        }
        else
        {
          type_decoder<Member> decoder;
          decoder.decode();
        }
      }
    }
  };

  class type_checksum_calculator
  {
  public:
    type_checksum_calculator(std::vector<member_data> &_members) : members(_members) {}
    template <typename Member, class Class, Member(Class::*member)>
    void operator()(const char *name) const
    {
      checksum += pos + sizeof(Member);
      ++pos;
      std::string member_type = fc::get_typename<Member>::name();
      members.emplace_back(member_data{.type_or_value = member_type, .name = name});
    }

    size_t get_checksum() { return checksum; }

  private:
    std::vector<member_data> &members;
    mutable size_t checksum = 0;
    mutable size_t pos = 0;
  };

  class enum_checksum_calculator
  {
  public:
    enum_checksum_calculator(std::vector<member_data> &_values) : values(_values) {}

    void operator()(const char *name, const int64_t value) const
    {
      checksum += std::hash<std::string>{}(name) + value;
      values.emplace_back(member_data{.type_or_value = std::to_string(value), .name = name});
    }

    size_t get_checksum() { return checksum; }

  private:
    std::vector<member_data> &values;
    mutable size_t checksum = 0;
  };
}

template<typename T>
class enum_decoder<T, true /* is defined */, true /* is enum*/>
{
public:
  enum_decoder() : name(fc::get_typename<T>::name()) {}
  void decode() const
  {
    std::cerr << "Decoding enum: " << name << " ...\n";
    std::vector<member_data> values;
    visitors::enum_checksum_calculator checksum_calculator(values);
    fc::reflector<T>::visit(checksum_calculator);
    size_t checksum = checksum_calculator.get_checksum();
    decoded_types.try_emplace(name, decoded_type(checksum, name, std::move(values), true));
    std::cerr << "Decoding enum: " << name << " has finished.\n";
  }

private:
  std::string_view name;
};

template<typename T>
class type_decoder<T, true /* is defined */, false /* is enum*/>
{
  public:
    type_decoder() : name(fc::get_typename<T>::name()) {}
    void decode() const
    {
      std::cerr << "Decoding: " << name << " ...\n";
      detect_defined_types();
      decode_type();
      std::cerr << "Decoding: " << name << " has finished.\n";
    }

  private:
    std::string_view name;

    void detect_defined_types() const
    {
      fc::reflector<T>::visit(visitors::defined_types_detector());
    }

    void decode_type() const
    {
      std::vector<member_data> members;
      visitors::type_checksum_calculator checksum_calculator(members);
      fc::reflector<T>::visit(checksum_calculator);
      size_t checksum = checksum_calculator.get_checksum();
      decoded_types.try_emplace(name, decoded_type(checksum, name, std::move(members), false));
    }
};

template<typename T>
void add_type_to_decode()
{
  std::string_view name = fc::get_typename<T>::name();

  if (decoded_types.find(name) != decoded_types.end())
  {
    std::cerr << "Type '" << name << " already decoded, skipping ...\n";
    return;
  }

  type_decoder<T> decoder;
  decoder.decode();
}

int main( int argc, char** argv )
{
  try
  {
    add_type_to_decode<hive::chain::account_object>();
    add_type_to_decode<hive::chain::witness_object>();

    std::cerr << "\n----- Results: ----- \n\n";
    for (const auto& [key, val] : decoded_types)
      val.print_type_info();
  }
  catch ( const fc::exception& e )
  {
    edump((e.to_detail_string()));
  }

  return 0;
}
