#pragma once

#include <hive/account_statistics/account_statistics_plugin.hpp>

#include <fc/api.hpp>

namespace hive { namespace app {
   struct api_context;
} }

namespace hive { namespace account_statistics {

namespace detail
{
   class account_statistics_api_impl;
}

class account_statistics_api
{
   public:
      account_statistics_api( const hive::app::api_context& ctx );

      void on_api_startup();

   private:
      std::shared_ptr< detail::account_statistics_api_impl > _my;
};

} } // hive::account_statistics

FC_API( hive::account_statistics::account_statistics_api, )
