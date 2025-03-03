#pragma once

#include <fc/io/datastream.hpp>
#include <fc/io/raw_fwd.hpp>
#include <fc/io/json.hpp>

namespace chainbase
{
  class dumper_custom_data
  {
    public:

      dumper_custom_data()
      {

      }

      virtual void write( const void* obj )
      {

      }

      template<class MultiIndexType>
      void dump_object( const MultiIndexType& object )
      {
        write( &object );
        //const std::string json = fc::json::to_pretty_string(object);
        //ilog( json );
      }
  };
}