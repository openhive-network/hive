#pragma once

#include <core/utilities.hpp>

namespace beekeeper
{
  class beekeeper_instance_base
  {
    public:

      virtual ~beekeeper_instance_base(){}

      virtual bool start(){ return true; }
  };
}
