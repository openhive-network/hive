#pragma once

#include <appbase/application.hpp>

#include <core/beekeeper_app_init.hpp>
#include <core/beekeeper_wallet_api.hpp>

namespace beekeeper {

class beekeeper_app: public beekeeper_app_init
{
  private:

    appbase::initialization_result::result init_status = appbase::initialization_result::result::ok;

    boost::signals2::connection webserver_connection;

    std::unique_ptr<beekeeper::beekeeper_wallet_api> api_ptr;

    appbase::application& app;

    const boost::program_options::variables_map& get_args() const override;
    bfs::path get_data_dir() const override;
    void setup_notifications( const boost::program_options::variables_map& args ) override;

  protected:

    std::pair<bool, bool> initialize( int argc, char** argv ) override;
    bool start() override;

  public:

    beekeeper_app();
    ~beekeeper_app() override;
};

}