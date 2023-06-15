#pragma once

#include <core/beekeeper_app_init.hpp>
#include <extension/beekeeper_wallet_api.hpp>

namespace beekeeper {

class beekeeper_app: public beekeeper_app_init
{
  private:

    appbase::initialization_result::result init_status = appbase::initialization_result::result::ok;

    boost::signals2::connection webserver_connection;

    std::unique_ptr<beekeeper::beekeeper_wallet_api> api_ptr;

  protected:

    std::pair<appbase::initialization_result::result, bool> initialize( int argc, char** argv ) override;
    appbase::initialization_result::result start() override;

  public:

    beekeeper_app();
    ~beekeeper_app() override;
};

}