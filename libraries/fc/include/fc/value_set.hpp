#pragma once

#include <fc/io/json.hpp>

#include<string>
#include<vector>

namespace fc {

template<typename T>
T dejsonify(const std::string& s)
{
  return fc::json::from_string(s, fc::json::format_validation_mode::full).as<T>();
}

template<typename Expected_Final_Type, typename Type_Options, typename Container_Type>
void load_value_set( const Type_Options& options, const std::string& name, Container_Type& container )
{
  if( options.count(name) )
  {
    const auto& ops = options[name]. template as<std::vector<std::string>>();
    std::transform( ops.begin(), ops.end(), std::inserter(container, container.end()), &fc::dejsonify<Expected_Final_Type> );
  }
}

}