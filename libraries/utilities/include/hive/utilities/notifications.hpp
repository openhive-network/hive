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

  collector_t() = default;
  collector_t(collector_t&& collector) = default;

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

  notification_t(const fc::string &name, collector_t&& collector):
    collector_t{std::forward<collector_t>( collector )},
    time{fc::time_point::now()},
    name{name}
  {
  }
};

bool check_is_flag_set(const boost::program_options::variables_map &args);
void add_program_options(boost::program_options::options_description &options);
std::vector<fc::string> setup_notifications(const boost::program_options::variables_map &args);

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

    void register_endpoints( const std::vector<fc::ip::endpoint> &pool );
  };

public:
  void broadcast(const notification_t &notification)
  {
    if (!is_broadcasting_active())
      return;

    on_send(notification);
  }
  void setup(const std::vector<std::string> &address_pool);
  void register_endpoints( const std::vector<std::string> &pool );

private:
  signal_t on_send;
  std::unique_ptr<network_broadcaster> network;

  bool is_broadcasting_active() const;
  std::vector<fc::ip::endpoint> create_endpoints( const std::vector<std::string>& address_pool );
};

} // detail

class notification_handler_wrapper
{
  private:

    detail::notification_handler handler;

  public:

    void broadcast( const notification_t& ev ) noexcept
    {
      handler.broadcast( ev );
    }

    void setup( const std::vector<std::string>& address_pool )
    {
      handler.setup( address_pool );
    }

    void register_endpoint( const std::optional<std::string>& endpoint )
    {
      if( endpoint )
        register_endpoints( std::vector<std::string>{ endpoint.value() } );
      else
        register_endpoints( std::vector<std::string>() );
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
