#pragma once

#include <fc/time.hpp>
#include <fc/variant.hpp>
#include <fc/reflect/reflect.hpp>
#include <fc/reflect/variant.hpp>

#include <functional>
#include <map>
#include <mutex>
#include <shared_mutex>
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

struct webserver_item: public collectable_item
{
  using port_t = uint16_t;
  using address_t = fc::string;
  using webserver_type_t = fc::string;

  webserver_type_t type; // not serialized
  address_t address;
  port_t port;

  webserver_item(const webserver_type_t& type, const address_t& address, const port_t port)
    :type{type}, address{address}, port{port}
  {}
};

struct information_item: public collectable_item
{
  using key_t = fc::string;
  using value_t = fc::variant;
  using variant_map_t = std::map<key_t, value_t>;

  fc::string name;
  variant_map_t record;

  template <typename... Values>
  explicit information_item(const key_t& name, Values &&...values)
    :name{name}
  {
    static_assert(sizeof...(values) % 2 == 0, "Inconsistency in amount of variadic arguments, expected even number ( series of pairs: [ fc::string, fc::variant ] )");
    assign_values(values...);
  }

private:

  template <typename... Values>
  void assign_values(key_t key, value_t value, Values &&...values)
  {
    auto it = record.try_emplace(key, value);
    FC_ASSERT( it.second, "Duplicated key in map" );

    assign_values(values...);
  }

  // Specialization for end recursion
  void assign_values()
  {
  }
};

struct fork_item : public collectable_item
{
  using block_num_t = uint32_t;
  using block_id_t = fc::string;

  block_num_t new_head_block_num;
  block_id_t new_head_block_id;

  fork_item(const block_num_t block_num, const block_id_t& block_id)
    :new_head_block_num{block_num}, new_head_block_id{block_id}
  {}
};

struct statuses
{
  collectable_item::timestamp_t last_update{ fc::time_point::now() };
  std::map<information_item::key_t, information_item::value_t> records{};
  std::map<webserver_item::webserver_type_t, webserver_item> webservers{};
  std::vector<status_item> statuses{};
  std::vector<fork_item> forks{};
};

struct threadsafe_statuses
{
  statuses data;
  std::shared_mutex read_mtx;

  void update(const information_item& item)
  {
    std::shared_lock<std::shared_mutex> _{read_mtx};
    update_last_update(item);
    safely_update(records_mtx, [&](){ this->data.records.insert_or_assign(item.name, item.record); });
  }

  void update(const webserver_item& item)
  {
    std::shared_lock<std::shared_mutex> _{read_mtx};
    update_last_update(item);
    safely_update(webservers_mtx, [&](){ this->data.webservers.insert_or_assign(item.type, item); });
  }

  void update(const status_item& item)
  {
    std::shared_lock<std::shared_mutex> _{read_mtx};
    update_last_update(item);
    safely_update(statuses_mtx, [&](){ this->data.statuses.emplace_back(item); });
  }

  void update(const fork_item& item)
  {
    std::shared_lock<std::shared_mutex> _{read_mtx};
    update_last_update(item);
    safely_update(forks_mtx, [&](){ this->data.forks.emplace_back(item); });
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


} // utilities

} // hive

FC_REFLECT(hive::utilities::collectable_item, (timestamp));
FC_REFLECT_DERIVED(hive::utilities::status_item, (hive::utilities::collectable_item), (status));
FC_REFLECT_DERIVED(hive::utilities::webserver_item, (hive::utilities::collectable_item), (address)(port));
FC_REFLECT_DERIVED(hive::utilities::fork_item, (hive::utilities::collectable_item), (new_head_block_num)(new_head_block_id));
FC_REFLECT(hive::utilities::statuses, (last_update)(records)(statuses)(webservers)(forks));
