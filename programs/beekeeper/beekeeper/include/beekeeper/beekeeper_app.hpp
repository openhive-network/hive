#pragma once

#include <appbase/application.hpp>

#include <beekeeper/beekeeper_instance.hpp>
#include <beekeeper/beekeeper_wallet_api.hpp>

#include <core/beekeeper_app_base.hpp>

namespace beekeeper {

class beekeeper_app: public beekeeper_app_base
{
  private:

    bool start_loop = true;

    std::string notifications_endpoint;

    std::shared_ptr<beekeeper_instance> instance;

    boost::signals2::connection webserver_connection;

    std::unique_ptr<beekeeper::beekeeper_wallet_api> api_ptr;

    appbase::application app;

    void process_notifications( const boost::program_options::variables_map& args );
    std::string check_version();
    bool save_keys( const std::string& notification, const std::string& wallet_name, const std::string& wallet_password );

    const boost::program_options::variables_map& get_args() const override;
    bfs::path get_data_dir() const override;
    void setup_notifications( const boost::program_options::variables_map& args ) override;
    init_data save_keys( const boost::program_options::variables_map& args ) override;

    std::shared_ptr<beekeeper::beekeeper_wallet_manager> create_wallet( const boost::filesystem::path& cmd_wallet_dir, uint64_t cmd_unlock_timeout, uint32_t cmd_session_limit ) override;

    bool should_start_loop() const override;

  protected:

    void set_program_options() override;
    init_data initialize( int argc, char** argv ) override;
    void start() override;

  public:

    beekeeper_app();
    ~beekeeper_app() override;

    init_data init( int argc, char** argv ) override;
};

}