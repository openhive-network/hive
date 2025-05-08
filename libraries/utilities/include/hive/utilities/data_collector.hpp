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
#include <mutex>
#include <shared_mutex>
#include <utility>
#include <vector>

namespace hive { namespace utilities {

struct collectable_item
{
  using timestamp_t = fc::time_point_sec;

  timestamp_t timestamp;

  collectable_item(): timestamp{fc::time_point::now()} {}
  explicit collectable_item(timestamp_t time): timestamp{time} {}
};

struct status_item: public collectable_item
{
  using status_name_t = fc::string;

  status_name_t status;

  explicit status_item(const status_name_t& status)
    :status{status}
  {}
};

struct statuses
{
  collectable_item::timestamp_t last_update{ fc::time_point::now() };
  std::vector<status_item> statuses{};
};

struct threadsafe_statuses
{
  statuses data;
  std::shared_mutex read_mtx;

  void update(const status_item& item)
  {
    std::shared_lock<std::shared_mutex> _{read_mtx};
    update_last_update(item);
    safely_update(statuses_mtx, [&](){ this->data.statuses.emplace_back(item); });
  }

private:

  std::mutex last_update_mtx;
  std::mutex records_mtx;
  std::mutex webservers_mtx;
  std::mutex statuses_mtx;
  std::mutex forks_mtx;

  void update_last_update(const collectable_item& item)
  {
    safely_update(last_update_mtx, [&](){ this->data.last_update = item.timestamp; });
  }

  void safely_update(std::mutex& mtx, std::function<void()> update_function)
  {
    std::lock_guard<std::mutex> guard{mtx};
    update_function();
  }
};

struct statuses_signal_manager
{
  using new_status_handler_t = std::function<void(const status_item&)>;

  using new_status_signal_t = boost::signals2::signal<void(const status_item&)>;

  using connection_t = boost::signals2::connection;

  connection_t add_new_status_handler( const new_status_handler_t& func ) { return new_status_signal.connect(func); }

  void save_status(const status_item::status_name_t& status_name) const
  {
    if(!new_status_signal.empty())
      new_status_signal(status_item{status_name});
  }

private:

  new_status_signal_t new_status_signal{};
};


} // utilities

} // hive

FC_REFLECT(hive::utilities::collectable_item, (timestamp));
FC_REFLECT_DERIVED(hive::utilities::status_item, (hive::utilities::collectable_item), (status));
FC_REFLECT(hive::utilities::statuses, (last_update)(statuses));