#pragma once

#include <fc/time.hpp>
#include <fc/variant.hpp>
#include <fc/io/json.hpp>
#include <fc/network/ip.hpp>
#include <fc/reflect/reflect.hpp>
#include <fc/reflect/variant.hpp>

#include <boost/program_options.hpp>
#include <boost/signals2.hpp>

#include <atomic>
#include <iostream>
#include <map>
#include <future>
#include <utility>
#include <vector>
#include <regex>

namespace hive { namespace utilities { namespace notifications {

class collector_t
{
public:

  using key_t   = fc::string;
  using value_t = fc::variant;

  std::map<key_t, value_t> value;

  collector_t() = default;
  collector_t(collector_t&& collector) = default;

  template <typename... Values>
  collector_t(Values &&...values)
  {
    static_assert(sizeof...(values) % 2 == 0, "Inconsistency in amount of variadic arguments, expected even number ( series of pairs: [ fc::string, fc::variant ] )");
    assign_values(values...);
  }

  template <typename... Values>
  void assign_values(key_t key, const value_t& value, Values &&...values)
  {
    auto it = this->value.emplace(key, value);
    FC_ASSERT( it.second, "Duplicated key in map" );

    assign_values(values...);
  }

  template <typename any_value_t, typename... Values>
  void assign_values(key_t key, const any_value_t& value, Values &&...values)
  {
    assign_values(key, value_t{value}, values...);
  }

private:
  // Specialization for end recursion
  void assign_values()
  {
  }
};

class notification_t : public collector_t
{
public:

  fc::time_point_sec time;
  fc::string name;

  template <typename... Values>
  notification_t(const fc::string &name, Values &&...values):
    collector_t{values...},
    time{fc::time_point::now()},
    name{name}
  {
  }

  notification_t(const fc::string &name, collector_t&& collector):
    collector_t{std::forward<collector_t>( collector )},
    time{fc::time_point::now()},
    name{name}
  {
  }
};

bool check_is_notifications_enabled(const boost::program_options::variables_map &args);
bool check_is_notification_filter_provided(const boost::program_options::variables_map &args);
void add_program_options(boost::program_options::options_description &options);
std::tuple<std::vector<fc::string>, fc::string> setup_notifications(const boost::program_options::variables_map &args);

namespace detail {

constexpr uint32_t MAX_RETRIES = 5;
bool error_handler(const std::function<void()> &foo);

class notification_handler
{
  using signal_connection_t = boost::signals2::connection;
  using signal_t = boost::signals2::signal<void(const notification_t &)>;
  using variant_map_t = std::map<fc::string, fc::variant>;

  class network_broadcaster
  {
    class notification_connection_holder_t
    {
      fc::ip::endpoint m_address;
      std::shared_ptr<std::mutex> m_mtx_connection_access{ std::make_shared<std::mutex>() };
      uint8_t m_failure_counter{0};

    public:
      explicit notification_connection_holder_t(const fc::ip::endpoint& ip) : m_address{ip} {}
      void send_request(const notification_t& note);
    };

    std::atomic_bool allowed_connection{true};
    std::vector<notification_connection_holder_t> connections;

    bool allowed() const { return allowed_connection.load(); }

  public:
    explicit network_broadcaster(const std::vector<fc::ip::endpoint> &pool)
    {
      connections.reserve(size_t{10ul});
      for (const fc::ip::endpoint &addr : pool)
        connections.emplace_back(addr);
    }

    void broadcast(const notification_t &ev) noexcept;

    void register_endpoints( const std::vector<fc::ip::endpoint> &pool );
  };

  static void broadcast_impl(network_broadcaster& broadcaster, std::unique_ptr<notification_t> note);

public:
  template <typename... KeyValuesTypes>
  void broadcast(
      const fc::string name,
      KeyValuesTypes&& ...key_value_pairs) noexcept
  {
    if (!is_broadcasting_active())
      return;

    /*
      +------------+----------+----------+
      |    XOR     | match[F] | match[T] |
      +------------+----------+----------+
      | exclude[F] |   STOP   |   SEND   |
      +------------+----------+----------+
      | exclude[T] |   SEND   |   STOP   |
      +------------+----------+----------+
    */
    if(this->name_filter.valid() && !(std::regex_match(name, *this->name_filter) ^ this->exclude_matching))
      return;

    {
      std::unique_lock<std::mutex> lck{mtx_access_futures};

      if(futures.size() == max_amount_of_futures)
      {
        futures.clear();
        futures.reserve(max_amount_of_futures);
      }

      auto note_ptr = std::make_unique<notification_t>(name, std::forward<KeyValuesTypes>(key_value_pairs)...);
      futures.emplace_back(
        std::async(
          std::launch::async,
          &notification_handler::broadcast_impl,
          std::ref(*network),
          std::move(note_ptr)
        )
      );
    }
  }
  void setup(const std::vector<std::string> &address_pool, const fc::string& regex);
  void register_endpoints( const std::vector<std::string> &pool );

private:
  inline static const size_t max_amount_of_futures{ 100ul };
  inline const static fc::string::value_type exclude_matching_sign{'!'};

  std::unique_ptr<network_broadcaster> network;
  fc::optional<std::regex> name_filter;
  bool exclude_matching;

  std::vector<std::future<void>> futures;
  std::mutex mtx_access_futures{};

  bool is_broadcasting_active() const;
  std::vector<fc::ip::endpoint> create_endpoints( const std::vector<std::string>& address_pool );
};
} // detail

class notification_handler_wrapper
{
  private:

    detail::notification_handler handler;

  public:

  template <typename... KeyValuesTypes>
  void broadcast(
      const fc::string &name,
      KeyValuesTypes &&...key_value_pairs) noexcept
  {
    handler.broadcast(name, key_value_pairs...);
  }

    void setup( const std::vector<std::string>& address_pool, const fc::string& regex )
    {
      handler.setup( address_pool, regex );
    }

    void register_endpoints( const std::vector<std::string>& pool )
    {
      handler.register_endpoints( pool );
    }
};

bool error_handler( const std::function<void()>& foo );

} } // utilities::notifications

} // hive

FC_REFLECT(hive::utilities::notifications::collector_t, (value));
FC_REFLECT_DERIVED(hive::utilities::notifications::notification_t, (hive::utilities::notifications::collector_t), (time)(name));
