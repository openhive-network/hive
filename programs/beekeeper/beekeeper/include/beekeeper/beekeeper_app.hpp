#pragma once

#include <appbase/application.hpp>

#include <beekeeper/beekeeper_instance.hpp>
#include <beekeeper/beekeeper_wallet_api.hpp>
#include <beekeeper/mutex_handler.hpp>

#include <core/beekeeper_app_base.hpp>

namespace beekeeper {

class beekeeper_app: public beekeeper_app_base
{
  private:

    bool start_loop = true;

    uint64_t unlock_interval = 0;

    std::shared_ptr<beekeeper_instance> instance;

    std::unique_ptr<beekeeper::beekeeper_wallet_api> api_ptr;

    appbase::application app;

    std::shared_ptr<mutex_handler> mtx_handler;

    std::string check_version();
    uint32_t save_keys( const std::string& wallet_name, const std::string& wallet_password );

    const boost::program_options::variables_map& get_args() const override;
    bfs::path get_data_dir() const override;
    void setup( const boost::program_options::variables_map& args ) override;
    uint32_t save_keys( const boost::program_options::variables_map& args ) override;

    std::shared_ptr<beekeeper::beekeeper_wallet_manager> create_wallet( const boost::filesystem::path& cmd_wallet_dir, uint64_t cmd_unlock_timeout, uint32_t cmd_session_limit ) override;

    bool should_start_loop() const override;

  protected:

    void set_program_options() override;
    uint32_t initialize( int argc, char** argv ) override;
    void start() override;

  public:

    beekeeper_app();
    ~beekeeper_app() override;

    uint32_t init( int argc, char** argv ) override;
};

}