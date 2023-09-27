#pragma once

#include <core/beekeeper_app_base.hpp>
#include <core/beekeeper_wallet_manager.hpp>

#include <boost/program_options.hpp>

namespace beekeeper {

namespace bpo = boost::program_options;
namespace bfs = boost::filesystem;

class beekeeper_app_init : public beekeeper_app_base
{
  private:

    const uint32_t session_limit = 64;

  protected:

    bpo::options_description                              options;
    std::shared_ptr<beekeeper::beekeeper_wallet_manager>  wallet_manager_ptr;

    void set_program_options() override;

  protected:

    bool save_keys( const std::string& notification, const std::string& wallet_name, const std::string& wallet_password ) override;

    virtual std::string get_notifications_endpoint( const boost::program_options::variables_map& args );
    init_data initialize_program_options();
    std::string check_version();

    virtual const boost::program_options::variables_map& get_args() const = 0;
    virtual bfs::path get_data_dir() const = 0;
    virtual void setup_notifications( const boost::program_options::variables_map& args ) {};
    virtual void setup_logger( const boost::program_options::variables_map& args ) {};

    virtual std::shared_ptr<beekeeper::beekeeper_wallet_manager> create_wallet( const boost::filesystem::path& cmd_wallet_dir, uint64_t cmd_unlock_timeout, uint32_t cmd_session_limit ) = 0;

  public:

    beekeeper_app_init();
    ~beekeeper_app_init() override;

    std::shared_ptr<beekeeper::beekeeper_wallet_manager> get_wallet_manager()
    {
      return wallet_manager_ptr;
    }
};

}
