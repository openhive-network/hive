#include <hive/chain/account_object.hpp>

#include <iostream>
#include <sstream>
#include <unordered_map>

struct member_data
{
  std::string_view type;
  std::string_view name;
};

class decoded_type
{
  public:
    decoded_type(const size_t _checksum, const std::string_view _name, std::vector<member_data>&& _members) : checksum(_checksum), name(_name), members(std::move(_members)) {}
    void print_type_info() const;

  private:
    size_t checksum = 0;
    std::string_view name;
    std::vector<member_data> members;
};

void decoded_type::print_type_info() const
{
  std::stringstream ss;
  ss << "Type: " << name << "\n";
  ss << "Checksum: " << checksum << "\n";
  ss << "Members:" << "\n";
  for (const auto& member : members)
    ss << "  " << member.name << ": " << member.type << "\n";

  ss << "\n";

  std::cerr << ss.str();
}

std::unordered_set<std::string_view> types_being_decoded;
std::unordered_map<std::string_view, decoded_type> decoded_types;

template<typename T, bool defined = fc::reflector<T>::is_defined::value>
class type_decoder;

template<typename T>
class type_decoder<T, false>
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
        type_decoder<Member> decoder;
        decoder.decode();
      }
    }
  };

  class checksum_calculator
  {
  public:
    checksum_calculator(std::vector<member_data> &_members) : members(_members) {}
    template <typename Member, class Class, Member(Class::*member)>
    void operator()(const char *name) const
    {
      checksum += pos + sizeof(Member);
      ++pos;
      std::string_view member_type = fc::get_typename<Member>::name();
      members.emplace_back(member_data{.type = member_type, .name = name});
    }

    size_t get_checksum() { return checksum; }

  private:
    std::vector<member_data> &members;
    mutable size_t checksum = 0;
    mutable size_t pos = 0;
  };
}

template<typename T, bool defined>
class type_decoder
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
      visitors::checksum_calculator checksum_calculator(members);
      fc::reflector<T>::visit(checksum_calculator);
      size_t checksum = checksum_calculator.get_checksum();
      decoded_types.try_emplace(name, decoded_type(checksum, name, std::move(members)));
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
