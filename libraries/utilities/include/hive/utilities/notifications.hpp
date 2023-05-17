#pragma once

#include <fc/time.hpp>
#include <fc/variant.hpp>
#include <fc/network/ip.hpp>
#include <fc/reflect/reflect.hpp>
#include <fc/reflect/variant.hpp>

#include <boost/program_options.hpp>
#include <boost/signals2.hpp>

#include <atomic>
#include <iostream>
#include <map>
#include <utility>
#include <vector>

namespace hive { namespace utilities { namespace notifications {

class collector_t
{
public:

  using key_t   = fc::string;
  using value_t = fc::variant;

  std::map<key_t, value_t> value;

  collector_t(){}

  collector_t(collector_t&& collector): value(collector.value) {}
  collector_t(const collector_t& collector): value(collector.value) {}
  collector_t(collector_t& collector): value(collector.value) {}

  template <typename... Values>
  collector_t(Values &&...values)
  {
    static_assert(sizeof...(values) % 2 == 0, "Inconsistency in amount of variadic arguments, expected even number ( series of pairs: [ fc::string, fc::variant ] )");
    assign_values(values...);
  }

  template <typename... Values>
  void assign_values(key_t key, value_t value, Values &&...values)
  {
    auto it = this->value.emplace(key, value);
    FC_ASSERT( it.second, "Duplicated key in map" );

    assign_values(values...);
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

  notification_t(const fc::string &name, const collector_t& collector):
    collector_t{collector},
    time{fc::time_point::now()},
    name{name}
  {
  }
};

bool check_is_flag_set(const boost::program_options::variables_map &args);
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
    signal_connection_t connection;
    std::atomic_bool allowed_connection{true};
    std::map<fc::ip::endpoint, uint32_t> address_pool;
    bool allowed() const { return allowed_connection.load(); }

  public:
    network_broadcaster(const std::vector<fc::ip::endpoint> &pool, signal_t &con)
    {
      for (const fc::ip::endpoint &addr : pool)
        address_pool[addr] = 0;
      this->connection = con.connect([&](const notification_t &ev) { this->broadcast(ev); });
    }

    ~network_broadcaster()
    {
      allowed_connection.store(false);
      if (connection.connected())
        connection.disconnect();
    }

    void broadcast(const notification_t &ev) noexcept;
  };

public:
  void broadcast(const notification_t &notification)
  {
    if (!is_broadcasting_active())
      return;

    on_send(notification);
  }
  void setup(const std::vector<fc::ip::endpoint> &address_pool);

private:
  signal_t on_send;
  std::unique_ptr<network_broadcaster> network;

  bool is_broadcasting_active() const;
};

} // detail

detail::notification_handler &get_notification_handler_instance();

} } // utilities::notifications

using namespace utilities::notifications;

template <typename... KeyValuesTypes>
inline void notify(
    const fc::string &name,
    KeyValuesTypes &&...key_value_pairs) noexcept
{
  detail::error_handler([&]{
    get_notification_handler_instance().broadcast(
      notification_t(name, std::forward<KeyValuesTypes>(key_value_pairs)...)
    );
  });
}

inline void notify(
    const fc::string &name,
    const collector_t& collector) noexcept
{

  detail::error_handler([&]{
    get_notification_handler_instance().broadcast(
      notification_t(name, collector)
    );
  });
}

void notify_hived_status(const fc::string &current_status) noexcept;
void notify_hived_error(const fc::string &error_message) noexcept;

} // hive

FC_REFLECT(hive::utilities::notifications::collector_t, (value));
FC_REFLECT_DERIVED(hive::utilities::notifications::notification_t, (hive::utilities::notifications::collector_t), (time)(name));
