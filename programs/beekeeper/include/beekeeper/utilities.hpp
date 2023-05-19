#pragma once

#include <fc/reflect/reflect.hpp>

#include <functional>
#include <string>

namespace beekeeper {

struct wallet_details
{
  std::string name;
  bool unlocked = false;
};

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

FC_REFLECT( beekeeper::wallet_details, (name)(unlocked) )
FC_REFLECT( beekeeper::info, (now)(timeout_time) )