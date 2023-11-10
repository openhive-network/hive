#pragma once

#include <fc/io/json.hpp>

namespace fc {

template<typename T>
T dejsonify(const std::string& s) {
  return fc::json::from_string(s).as<T>();
}

#ifndef LOAD_VALUE_SET
#define LOAD_VALUE_SET(options, name, container, type) \
if( options.count(name) ) { \
  const std::vector<std::string>& ops = options[name].as<std::vector<std::string>>(); \
  std::transform(ops.begin(), ops.end(), std::inserter(container, container.end()), &fc::dejsonify<type>); \
}
#endif

}