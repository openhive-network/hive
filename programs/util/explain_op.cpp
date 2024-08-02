#include <appbase/application.hpp>

#include <fc/log/logger.hpp>
#include <fc/log/logger_config.hpp>
#include <fc/log/file_appender.hpp>
#include <fc/crypto/hex.hpp>
#include <fc/io/json.hpp>
#include <fc/filesystem.hpp>
#include <fc/thread/thread.hpp>
#include <fc/string.hpp>
#include <hive/chain/block_log.hpp>
#include <hive/chain/detail/block_attributes.hpp>
#include <hive/chain/block_compression_dictionaries.hpp>
#include <hive/chain/full_block.hpp>
#include <hive/chain/blockchain_worker_thread_pool.hpp>
#include <hive/chain/block_compression_dictionaries.hpp>
#include <hive/utilities/io_primitives.hpp>
#include <hive/utilities/git_revision.hpp>
#include <hive/chain/buffer_type.hpp>

#include <thread>
#include <chrono>
#include <memory>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <algorithm>
#include <string>

std::string to_string(bool v);
std::string to_string(int16_t v);
std::string to_string(int64_t v);
std::string to_string(uint8_t v);
std::string to_string(uint16_t v);
std::string to_string(uint32_t v);
std::string to_string(uint64_t v);
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
template<typename T>
std::string to_string(const fc::optional<T>& v);
template<typename T, size_t N>
std::string to_string(const fc::array<T, N>& v);
std::string to_string(const std::vector<char>& v);
template<typename T>
std::string to_string(const std::vector<T>& v);
template<typename A, typename B>
std::string to_string(const std::pair<A, B>& v);
template<typename T>
std::string to_string(const boost::container::flat_set<T>& v);
template<typename T>
std::string to_string(const flat_set_ex<T>& v);
template<typename K, typename... T>
std::string to_string(const boost::container::flat_map<K, T...>& v);
template<typename... Types>
std::string to_string(const fc::static_variant<Types...>& v);
template<typename T>
std::string to_string(const T& v);


template<typename Op>
class member_explainer
{
public:
  member_explainer(const Op& op)
    : op(op)
  {}
  member_explainer(const Op& op, const std::string& prefix)
    : op(op), prefix(prefix+".")
  {}

    template<typename Member, class Class, Member (Class::*member)>
    void operator()(const char* name) const
    {
      if constexpr (fc::reflector<Member>::is_defined::value)
      {
        fc::reflector<Member>::visit(member_explainer<Member>(op.*member, prefix + name));
      }
      else
      {
        std::vector<char> v = fc::raw::pack_to_vector(op.*member);
        std::cout << fc::to_hex(v) << ": " << name << ": " << to_string(op.*member) << '\n';
      }
    }

private:
    const Op& op;
    std::string prefix;
};

struct explainer
{
  using result_type = void;

  template <typename Op>
  void operator()(const Op& op) const
  {
    fc::reflector<Op>::visit(member_explainer<Op>(op));
  }

};

std::string to_string(bool v)
{
  return v ? "true" : "false";
}

std::string to_string(int16_t v)
{
  return std::to_string(static_cast<int>(v));
}

std::string to_string(int64_t v)
{
  return std::to_string(static_cast<int>(v));
}

std::string to_string(uint8_t v)
{
  return std::to_string(static_cast<int>(v));
}

std::string to_string(uint16_t v)
{
  return std::to_string(static_cast<int>(v));
}

std::string to_string(uint32_t v)
{
  return std::to_string(static_cast<int>(v));
}

std::string to_string(uint64_t v)
{
  return std::to_string(static_cast<int>(v));
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

template<typename T>
std::string to_string(const fc::optional<T>& v)
{
  if (v.valid()) return to_string(*v);
  else return "";
}

template<typename T, size_t N>
std::string to_string(const fc::array<T, N>& v)
{
  return "fc::array"; // TODO
}

std::string to_string(const std::vector<char>& v)
{
  return "vec"; // TODO
}

template<typename T>
std::string to_string(const std::vector<T>& v)
{
  return "vec<T>"; // TODO
}

template<typename A, typename B>
std::string to_string(const std::pair<A, B>& v)
{
  return "pair<A, B>"; // TODO
}

template<typename T>
std::string to_string(const boost::container::flat_set<T>& v)
{
  return "flat_set<T>"; // TODO
}

template<typename T>
std::string to_string(const flat_set_ex<T>& v)
{
  return "flat_set_ex<T>"; // TODO
}

template<typename K, typename... T>
std::string to_string(const boost::container::flat_map<K, T...>& v)
{
  return "flat_map<K< T...>"; // TODO
}

template<typename... Types>
std::string to_string(const fc::static_variant<Types...>& v)
{
  std::vector<char> tag = fc::raw::pack_to_vector((uint8_t)v.which());
  std::cout << fc::to_hex(tag) << ": " << v.get_stored_type_name() << '\n';
  v.visit(explainer{});
  return "";
}

template<typename T>
std::string to_string(const T& v)
{
  static_assert(fc::reflector<T>::is_defined::value);
  fc::reflector<T>::visit(member_explainer<T>(v));
  return "";
}

int main()
{
  std::string hex;
  std::string buf;
  buf.resize(4096);
  for (;std::cin >> hex;) {
    fc::from_hex(hex, buf.data(), 4096);
    hive::protocol::operation op = fc::raw::unpack_from_buffer<hive::protocol::operation>(buf);
    to_string(op);
  }
}
