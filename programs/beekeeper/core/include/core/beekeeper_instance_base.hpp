#pragma once

#include <core/utilities.hpp>

namespace beekeeper
{
  class beekeeper_instance_base
  {
    public:

      virtual ~beekeeper_instance_base(){}

      virtual void change_app_status( const std::string& new_status )
      {
        //nothing to do
      };

      virtual std::optional<status> get_app_status() const
      {
        return std::optional<status>();
      }

      virtual bool start(){ return true; }
  };
}
