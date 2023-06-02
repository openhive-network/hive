#pragma once

#include <core/beekeeper_app_init.hpp>

namespace beekeeper {

class beekeeper_local_app: public beekeeper_app_init
{
  private:

    appbase::initialization_result::result init_status = appbase::initialization_result::result::ok;
  protected:

    std::pair<appbase::initialization_result::result, bool> initialize( int argc, char** argv ) override;
    appbase::initialization_result::result start() override;

  public:

    beekeeper_local_app();
    ~beekeeper_local_app() override;
};

}