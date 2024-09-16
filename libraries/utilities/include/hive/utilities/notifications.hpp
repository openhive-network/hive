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

  notification_t()
  {
  }

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

} } // utilities::notifications

} // hive

FC_REFLECT(hive::utilities::notifications::collector_t, (value));
FC_REFLECT_DERIVED(hive::utilities::notifications::notification_t, (hive::utilities::notifications::collector_t), (time)(name));
