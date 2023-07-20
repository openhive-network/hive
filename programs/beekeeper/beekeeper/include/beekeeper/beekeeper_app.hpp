#pragma once

#include <appbase/application.hpp>

#include <beekeeper/beekeeper_instance.hpp>

#include <core/beekeeper_app_init.hpp>
#include <core/beekeeper_wallet_api.hpp>

namespace beekeeper {

class beekeeper_app: public beekeeper_app_init
{
  private:

    std::shared_ptr<beekeeper_instance> instance;

    appbase::initialization_result::result init_status = appbase::initialization_result::result::ok;

    boost::signals2::connection webserver_connection;

    std::unique_ptr<beekeeper::beekeeper_wallet_api> api_ptr;

    appbase::application& app;

    const boost::program_options::variables_map& get_args() const override;
    bfs::path get_data_dir() const override;
    void setup_notifications( const boost::program_options::variables_map& args ) override;

    std::shared_ptr<beekeeper::beekeeper_wallet_manager> create_wallet( const boost::filesystem::path& cmd_wallet_dir, uint64_t cmd_unlock_timeout, uint32_t cmd_session_limit ) override;

  protected:

    void set_program_options() override;
    std::pair<bool, bool> initialize( int argc, char** argv ) override;
    bool start() override;

  public:

    beekeeper_app();
    ~beekeeper_app() override;
};

}