#pragma once

#include <fc/reflect/reflect.hpp>

#include <functional>
#include <string>

namespace beekeeper {

struct info
{
  std::string now;
  std::string timeout_time;
};

namespace types
{
  using notification_method_type = std::function<void()>;
  using lock_method_type = std::function<void(const std::string&)>;
}

}

FC_REFLECT( beekeeper::info, (now)(timeout_time) )