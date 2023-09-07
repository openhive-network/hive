#pragma once

#include <appbase/application.hpp>

#include <beekeeper/beekeeper_instance.hpp>
#include <beekeeper/beekeeper_wallet_api.hpp>

#include <core/beekeeper_app_init.hpp>

namespace beekeeper {

class beekeeper_app: public beekeeper_app_init
{
  private:

    std::string notifications_endpoint;

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

    std::string get_notifications_endpoint( const boost::program_options::variables_map& args ) override;
    void set_program_options() override;
    init_data initialize( int argc, char** argv ) override;
    void start() override;

  public:

    beekeeper_app();
    ~beekeeper_app() override;
};

}