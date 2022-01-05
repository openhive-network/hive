#pragma once

#include <fc/log/logger.hpp>

#include<chrono>

namespace fc
{
  class time_logger
  {
    private:

      bool     enabled  = false;
      uint64_t val      = 0;

      std::string info;
      std::string additional_info;

      void start_impl()
      {
        stop();

        val = std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::system_clock::now().time_since_epoch() ).count();
        enabled = true;
      }

    public:

      time_logger()
      {
      }

      time_logger( std::string _info, std::string _additional_info = "" )
                  : info( _info ), additional_info( _additional_info )
      {
        start();
      }

      ~time_logger()
      {
        stop();
      }

      void start()
      {
        start_impl();
      }

      void start( std::string _info, std::string _additional_info = "" )
      {
        start_impl();

        info            = _info;
        additional_info = _additional_info;
      }

      void stop( bool allow_display_message = true )
      {
        if( enabled )
        {
          if( allow_display_message )
          {
            uint64_t _val = std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::system_clock::now().time_since_epoch() ).count() - val;
            if( additional_info.empty() )
            {
              ilog("(${m}) ${t}[ms]", ( "m", info.c_str() )( "t", std::to_string(_val).c_str() ) );
            }
            else
            {
              ilog("(${m1})(${m2}) time: ${t}[ms]", ( "m1", info.c_str() )( "m2", additional_info.c_str() )( "t", std::to_string(_val).c_str() ) );
            }
          }

          enabled = false;
        }
      }

  };
}
