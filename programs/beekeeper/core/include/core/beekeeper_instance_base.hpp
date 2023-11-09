#pragma once

namespace beekeeper
{
  class beekeeper_instance_base
  {
    public:

      virtual ~beekeeper_instance_base(){}

      virtual bool start(){ return true; }
  };
}
