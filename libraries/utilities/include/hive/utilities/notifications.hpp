#pragma once

#include <fc/time.hpp>
#include <fc/variant.hpp>
#include <fc/network/ip.hpp>
#include <fc/reflect/reflect.hpp>
#include <fc/reflect/variant.hpp>

#include <boost/program_options.hpp>
#include <boost/signals2.hpp>
#include <iostream>

namespace hive
{
  namespace utilities
  {
    namespace notifications
    {
      struct notification_t
      {
        fc::time_point_sec time;
        fc::string name;
        fc::variant value;

        explicit notification_t(const fc::string &notifi_name, const fc::variant &event_data = fc::nullptr_t{})
            : time{fc::time_point::now()}, name{notifi_name}, value{event_data} {}
      };

      bool check_is_flag_set(const boost::program_options::variables_map &args);
      void add_program_options(boost::program_options::options_description &options);
      void process_program_options(const boost::program_options::variables_map &args);

      namespace detail
      {
        constexpr uint32_t MAX_RETRIES = 5;
        bool error_handler(std::function<void()> foo);

        class notification_handler
        {
          using signal_connection_t = boost::signals2::connection;
          using signal_t = boost::signals2::signal<void(const notification_t &)>;
          using variant_map_t = std::map<fc::string, fc::variant>;

          struct ___null_t
          {
          };
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
          template <typename... Argv>
          void broadcast(const fc::string &notifi_name, const fc::string &key, const fc::variant &var, Argv &&...args)
          {
            static_assert(sizeof...(args) % 2 == 0, "inconsistency in amount of variadic arguments, expected even number ( series of pairs: [ fc::string, fc::variant ] )");
            if (!is_broadcasting_active())
              return;

            variant_map_t values;
            broadcast_impl(values, key, var, std::forward<Argv>(args)..., ___null_t{});
            on_send(notification_t{notifi_name, fc::variant_object{values}});
          }
          void broadcast(const fc::string &notifi_name);
          void setup(const std::vector<fc::ip::endpoint> &address_pool);

        private:
          signal_t on_send;
          std::unique_ptr<network_broadcaster> network;

          template <typename... Argv>
          void broadcast_impl(variant_map_t &object, const fc::string &key, const fc::variant &var, Argv &&...args)
          {
            add_variant(object, key, var);
            broadcast_impl(object, std::forward<Argv>(args)...);
          }
          void broadcast_impl(variant_map_t &object, const fc::string &key, const fc::variant &var, const ___null_t);
          void add_variant(variant_map_t &object, const fc::string &key, const fc::variant &var);
          bool is_broadcasting_active() const;
        };
      } // detail

      detail::notification_handler &get_notification_handler_instance();
    } // notifications
  } // utilities

  template <typename... KeyValuesTypes>
  inline void notify(
      const fc::string &name,
      const fc::string &key = fc::string{},
      const fc::variant &var = fc::variant{fc::nullptr_t{}},
      KeyValuesTypes &&...key_value_pairs) noexcept
  {
    utilities::notifications::detail::error_handler([&]{
      if (key.empty())
        utilities::notifications::get_notification_handler_instance().broadcast(name);
      else
        utilities::notifications::get_notification_handler_instance().broadcast(
          name, key, var, std::forward<KeyValuesTypes>(key_value_pairs)...
        );
    });
  }

  void notify_hived_status(const fc::string &current_status) noexcept;
} // hive

FC_REFLECT(hive::utilities::notifications::notification_t, (time)(name)(value));
