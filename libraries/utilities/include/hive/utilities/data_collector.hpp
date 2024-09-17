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

namespace hive { namespace utilities {

class data_collector
{
public:

  using key_t   = fc::string;
  using value_t = fc::variant;

  fc::time_point_sec time;
  fc::string name;

  std::map<key_t, value_t> value;

  data_collector()
  {
  }

  template <typename... Values>
  data_collector(const fc::string &name, Values &&...values):
    time{fc::time_point::now()},
    name{name}
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


} // utilities

} // hive

FC_REFLECT(hive::utilities::data_collector, (time)(name)(value));
