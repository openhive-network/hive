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
  using method_type = std::function<void()>;
}

}

FC_REFLECT( beekeeper::info, (now)(timeout_time) )