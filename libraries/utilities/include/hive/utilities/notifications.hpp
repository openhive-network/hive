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

class notification_t
{
public:

  using key_t =   ::fc::string;
  using value_t = ::fc::variant;

  fc::time_point_sec time;
  fc::string name;
  std::map<key_t, value_t> value;

  template <typename... Values>
  notification_t(const fc::string &name, Values &&...values):
    time{fc::time_point::now()},
    name{name}
  {
    static_assert(sizeof...(values) % 2 == 0, "Inconsistency in amount of variadic arguments, expected even number ( series of pairs: [ fc::string, fc::variant ] )");
    assign_values(values...);
  }

private:

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

  // Specialization for end recursion
  void assign_values()
  {
  }
};

bool check_is_notifications_enabled(const boost::program_options::variables_map &args);
void add_program_options(boost::program_options::options_description &options);
void setup_notifications(const boost::program_options::variables_map &args);

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
  void setup(const std::vector<fc::ip::endpoint> &address_pool, const fc::string& regex);

private:
  inline static const size_t max_amount_of_futures{ 100ul };
  inline const static fc::string::value_type exclude_matching_sign{'!'};

  std::unique_ptr<network_broadcaster> network;
  fc::optional<std::regex> name_filter;
  bool exclude_matching;

  std::vector<std::future<void>> futures;
  std::mutex mtx_access_futures{};

  bool is_broadcasting_active() const;
};

struct scope_guarded_timer
{
  fc::string timer_name;
  fc::time_point start;

  void reset();
  ~scope_guarded_timer();

private:
  void send_notif();
};

} // detail

detail::notification_handler &get_notification_handler_instance();

} } // utilities::notifications

template <typename... KeyValuesTypes>
inline void notify(
    const fc::string &name,
    KeyValuesTypes &&...key_value_pairs) noexcept
{
  using namespace utilities::notifications;

  detail::error_handler([&]{
    get_notification_handler_instance().broadcast(name, std::forward<KeyValuesTypes>(key_value_pairs)...);
  });
}

void notify_hived_status(const fc::string &current_status) noexcept;
void notify_hived_error(const fc::string &error_message) noexcept;
hive::utilities::notifications::detail::scope_guarded_timer notify_hived_timer(
  const fc::string &timer_name,
  const fc::optional<fc::time_point> start = fc::optional<fc::time_point>{} /* fc::time_point::now() by default */
) noexcept;

} // hive

FC_REFLECT(hive::utilities::notifications::notification_t, (time)(name)(value));
