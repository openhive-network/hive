#pragma once

#include <hive/utilities/data_collector_types.hpp>

#include <boost/signals2.hpp>

#include <utility>

namespace hive { namespace utilities {

struct statuses_signal_manager
{
  using new_webserver_handler_t = std::function<void(const webserver_item&)>;
  using new_status_handler_t = std::function<void(const status_item&)>;
  using new_information_handler_t = std::function<void(const information_item&)>;
  using new_fork_handler_t = std::function<void(const fork_item&)>;

  using new_webserver_signal_t = boost::signals2::signal<void(const webserver_item&)>;
  using new_status_signal_t = boost::signals2::signal<void(const status_item&)>;
  using new_information_signal_t = boost::signals2::signal<void(const information_item&)>;
  using new_fork_signal_t = boost::signals2::signal<void(const fork_item&)>;

  using connection_t = boost::signals2::connection;

  connection_t add_new_webserver_handler( const new_webserver_handler_t& func ) { return new_webserver_signal.connect(func); }
  connection_t add_new_status_handler( const new_status_handler_t& func ) { return new_status_signal.connect(func); }
  connection_t add_new_information_handler( const new_information_handler_t& func ) { return new_information_signal.connect(func); }
  connection_t add_new_fork_handler( const new_fork_handler_t& func ) { return new_fork_signal.connect(func); }

  void save_webserver(const webserver_item::webserver_type_t& type, const webserver_item::address_t& address, const webserver_item::port_t port) const
  {
    if(!new_webserver_signal.empty())
      new_webserver_signal(webserver_item{type, address, port});
  }

  void save_status(const status_item::status_name_t& status_name) const
  {
    if(!new_status_signal.empty())
      new_status_signal(status_item{status_name});
  }

  template <typename... KeyValuesTypes>
  void save_information(const information_item::key_t& name, KeyValuesTypes&& ... values) const
  {
    if(!new_information_signal.empty())
      new_information_signal(information_item(name, std::forward<KeyValuesTypes>(values)...));
  }

  void save_fork(const fork_item::block_num_t block_num, const fork_item::block_id_t& block_id) const
  {
    if(!new_fork_signal.empty())
      new_fork_signal(fork_item{block_num, block_id});
  }

private:

  new_webserver_signal_t new_webserver_signal{};
  new_status_signal_t new_status_signal{};
  new_information_signal_t new_information_signal{};
  new_fork_signal_t new_fork_signal{};

};


} // utilities

} // hive
