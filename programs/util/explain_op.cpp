#include <hive/chain/buffer_type.hpp>
#include <hive/protocol/asset.hpp>
#include <hive/protocol/fixed_string.hpp>
#include <hive/protocol/json_string.hpp>
#include <hive/protocol/legacy_asset.hpp>
#include <hive/protocol/operations.hpp>

#include <fc/crypto/hex.hpp>
#include <fc/crypto/ripemd160.hpp>
#include <fc/io/varint.hpp>
#include <fc/string.hpp>

#include <iostream>
#include <string>
#include <vector>
#include <type_traits>

std::string to_string(bool v);
std::string to_string(int16_t v);
std::string to_string(int64_t v);
std::string to_string(uint8_t v);
std::string to_string(uint16_t v);
std::string to_string(uint32_t v);
std::string to_string(uint64_t v);
std::string to_string(const fc::unsigned_int& v);
template<size_t N>
std::string to_string(const hive::protocol::fixed_string<N>& v);
template<typename Storage>
std::string to_string(const hive::protocol::fixed_string_impl<Storage>& v);
std::string to_string(const hive::protocol::legacy_asset& v);
std::string to_string(const hive::protocol::legacy_hive_asset_symbol_type& v);
std::string to_string(const hive::protocol::asset& v);
std::string to_string(const hive::protocol::legacy_hive_asset& v);
std::string to_string(const hive::protocol::public_key_type& v);
std::string to_string(const hive::protocol::json_string& v);
std::string to_string(const std::string& v);
std::string to_string(const fc::time_point_sec& v);
std::string to_string(const fc::ripemd160& v);
std::string to_string(const fc::sha256& v);
template<typename T>
std::string to_string(const fc::safe<T>& v);
template<typename T, size_t N>
std::string to_string(const fc::array<T, N>& v);
std::string to_string(const std::vector<char>& v);

template <typename, typename = void>
struct is_to_string_invocable : std::false_type {};
template <typename T>
struct is_to_string_invocable<T, std::void_t<decltype(to_string(std::declval<T>()))>> : std::true_type {};
template <typename T>
inline constexpr bool is_to_string_invocable_v = is_to_string_invocable<T>::value;

template<typename T>
void explain_member(const std::string& name, const fc::optional<T>& v);
template<typename T>
auto explain_member(const std::string& name, const std::vector<T>& v) -> std::enable_if_t<not std::is_same_v<T, char>>;
template<typename T>
auto explain_member(const std::string& name, const boost::container::flat_set<T>& v);
template<typename T>
auto explain_member(const std::string& name, const flat_set_ex<T>& v);
template<typename A, typename B>
auto explain_member(const std::string& name, const std::pair<A, B>& v);
template<typename K, typename... T>
auto explain_member(const std::string& name, const boost::container::flat_map<K, T...>& v);
template<typename... Types>
void explain_member(const std::string& name, const fc::static_variant<Types...>& v);
template<typename T>
auto explain_member(const std::string& name, const T& v) -> std::enable_if_t<not is_to_string_invocable_v<T>>;
template<typename T>
auto explain_member(const std::string& name, const T& v) -> std::enable_if_t<is_to_string_invocable_v<T>>;
template<typename U, typename V>
void explain_member(const std::string& name, const U& u, const V& v);

template<typename Op>
class member_explainer
{
public:
  member_explainer(const Op& op)
    : op(op)
  {}
  member_explainer(const Op& op, const std::string& prefix)
    : op(op), prefix(prefix)
  {}

    template<typename Member, class Class, Member (Class::*member)>
    void operator()(const char* name) const
    {
      if (prefix.empty())
        explain_member(name, op.*member);
      else
        explain_member(prefix+"."+name, op.*member);
    }

private:
    const Op& op;
    std::string prefix;
};

struct explainer
{
public:
  using result_type = void;

  explainer()
  {}
  explainer(const std::string& prefix)
    : prefix(prefix)
  {}

  template <typename Op>
  void operator()(const Op& op) const
  {
    fc::reflector<Op>::visit(member_explainer<Op>(op, prefix));
  }

private:
    std::string prefix;
};


template<typename T>
void explain_member(const std::string& name, const fc::optional<T>& v)
{
  explain_member(name+".valid", v.valid());
  if (v.valid())
    explain_member(name+".value", v.value());
}

template<typename T>
auto explain_member(const std::string& name, const std::vector<T>& v) -> std::enable_if_t<not std::is_same_v<T, char>>
{
  explain_member(name+".size", static_cast<fc::unsigned_int>(v.size()));
  size_t idx = 0;
  for (const auto& elem : v)
    explain_member(name+"."+std::to_string(idx++), elem);
}

template<typename T>
auto explain_member(const std::string& name, const boost::container::flat_set<T>& v)
{
  explain_member(name+".size", static_cast<fc::unsigned_int>(v.size()));
  size_t idx = 0;
  for (const auto& elem : v)
    explain_member(name+"."+std::to_string(idx++), elem);
}

template<typename T>
auto explain_member(const std::string& name, const flat_set_ex<T>& v)
{
  explain_member(name, static_cast<boost::container::flat_set<T>>(v));
}

template<typename A, typename B>
auto explain_member(const std::string& name, const std::pair<A, B>& v)
{
  explain_member(name+".first", (v.first));
  explain_member(name+".second", (v.second));
}

template<typename K, typename... T>
auto explain_member(const std::string& name, const boost::container::flat_map<K, T...>& v)
{
  explain_member(name+".size", static_cast<fc::unsigned_int>(v.size()));
  size_t idx = 0;
  for (const auto& elem : v)
    explain_member(name+"."+std::to_string(idx++), elem);
}

template<typename... Types>
void explain_member(const std::string& name, const fc::static_variant<Types...>& v)
{
  explain_member(name, (uint8_t)v.which(), v.get_stored_type_name());
  v.visit(explainer(name));
}

template<typename T>
auto explain_member(const std::string& name, const T& v) -> std::enable_if_t<not is_to_string_invocable_v<T>>
{
  fc::reflector<T>::visit(member_explainer<T>(v, name));
}

template<typename T>
auto explain_member(const std::string& name, const T& v) -> std::enable_if_t<is_to_string_invocable_v<T>>
{
  std::vector<char> vec = fc::raw::pack_to_vector(v);
  if (name.empty())
    std::cout << fc::to_hex(vec) << ": " << to_string(v) << '\n';
  else
    std::cout << fc::to_hex(vec) << ": " << name << ": " << to_string(v) << '\n';
}

template<typename U, typename V>
void explain_member(const std::string& name, const U& u, const V& v)
{
  std::vector<char> vec = fc::raw::pack_to_vector(u);
  if (name.empty())
    std::cout << fc::to_hex(vec) << ": " << to_string(v) << '\n';
  else
    std::cout << fc::to_hex(vec) << ": " << name << ": " << to_string(v) << '\n';
}


std::string to_string(bool v)
{
  return v ? "true" : "false";
}

std::string to_string(int16_t v)
{
  return std::to_string(v);
}

std::string to_string(int64_t v)
{
  return std::to_string(v);
}

std::string to_string(uint8_t v)
{
  return std::to_string(static_cast<int>(v));
}

std::string to_string(uint16_t v)
{
  return std::to_string(v);
}

std::string to_string(uint32_t v)
{
  return std::to_string(v);
}

std::string to_string(uint64_t v)
{
  return std::to_string(v);
}

std::string to_string(const fc::unsigned_int& v)
{
  return std::to_string(v.value);
}

template<size_t N>
std::string to_string(const hive::protocol::fixed_string<N>& v)
{
  return static_cast<std::string>(v);
}

template<typename Storage>
std::string to_string(const hive::protocol::fixed_string_impl<Storage>& v)
{
  return static_cast<std::string>(v);
}

std::string to_string(const hive::protocol::legacy_asset& v)
{
  return v.to_string();
}

std::string to_string(const hive::protocol::asset& v)
{
  return to_string(hive::protocol::legacy_asset(v));
}

std::string to_string(const hive::protocol::legacy_hive_asset& v)
{
  return to_string(v.to_asset<false>());
}

std::string to_string(const hive::protocol::legacy_hive_asset_symbol_type& v)
{
  return "LLL";
}

template<uint32_t _SYMBOL>
std::string to_string(const hive::protocol::tiny_asset<_SYMBOL>& v)
{
  return to_string(v.to_asset());
}

std::string to_string(const hive::protocol::public_key_type& v)
{
  return std::string(v);
}

std::string to_string(const hive::protocol::json_string& v)
{
  return static_cast<std::string>(v);
}

std::string to_string(const std::string& v)
{
  return v;
}

std::string to_string(const fc::time_point_sec& v)
{
  return fc::string(v);
}

std::string to_string(const fc::ripemd160& v)
{
  const auto vec = std::vector<char>((const char*)&v, ((const char*)&v) + sizeof(v));
  return to_string(vec);
}

std::string to_string(const fc::sha256& v)
{
  const auto vec = std::vector<char>((const char*)&v, ((const char*)&v) + sizeof(v));
  return to_string(vec);
}

template<typename T>
std::string to_string(const fc::safe<T>& v)
{
  return to_string(v.value);
}

template<typename T, size_t N>
std::string to_string(const fc::array<T, N>& v)
{
  const auto vec = std::vector<char>( (const char*)&v, ((const char*)&v) + sizeof(v) );
  return to_string(vec);
}

std::string to_string(const std::vector<char>& v)
{
  return fc::to_hex(v);
}

void chop(std::string& s, char prefix)
{
  if (s.size() > 0 && s[0] == prefix)
    s = s.substr(1);
}

int main()
{
  std::string hex;
  std::string bytes;
  for (;std::cin >> hex;) {
    try {
      chop(hex, '\\');
      chop(hex, 'x');
      bytes.resize(hex.size() / 2);
      fc::from_hex(hex, bytes.data(), bytes.size());
      hive::protocol::operation op = fc::raw::unpack_from_buffer<hive::protocol::operation>(bytes);
      explain_member("", op);
      std::cout << '\n';
    }
    catch (const std::exception& e)
    {
      std::cerr << "Unexpected error: " << e.what() << '\n';
    }
    catch (const fc::exception& e)
    {
      std::cerr << "Unexpected error: " << e.to_detail_string() << '\n';
    }
    catch ( ...  )
    {
      std::cerr << "Unknown exception" << '\n';
    }
  }
}
