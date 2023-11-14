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

    init_data initialize_program_options();

    virtual const boost::program_options::variables_map& get_args() const = 0;
    virtual bfs::path get_data_dir() const = 0;
    virtual void setup_notifications( const boost::program_options::variables_map& args ) {};
    virtual void setup_logger( const boost::program_options::variables_map& args ) {};
    virtual init_data save_keys( const boost::program_options::variables_map& args ) { return { true, get_revision() }; };

    virtual std::shared_ptr<beekeeper::beekeeper_wallet_manager> create_wallet( const boost::filesystem::path& cmd_wallet_dir, uint64_t cmd_unlock_timeout, uint32_t cmd_session_limit ) = 0;

    std::string get_revision() const;

  public:

    beekeeper_app_init();
    ~beekeeper_app_init() override;

    std::shared_ptr<beekeeper::beekeeper_wallet_manager> get_wallet_manager()
    {
      return wallet_manager_ptr;
    }
};

}
