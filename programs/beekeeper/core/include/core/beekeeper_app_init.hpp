#pragma once

#include <core/beekeeper_app_base.hpp>
#include <core/beekeeper_wallet_manager.hpp>

#include <boost/program_options.hpp>

namespace beekeeper {

namespace bpo = boost::program_options;

class beekeeper_app_init : public beekeeper_app_base
{
  private:

    const uint32_t session_limit = 64;

  protected:

    bpo::options_description                              options;
    std::shared_ptr<beekeeper::beekeeper_wallet_manager>  wallet_manager_ptr;

  protected:

    void set_program_options() override;
    void save_keys( const std::string& token, const std::string& wallet_name, const std::string& wallet_password ) override;
    std::pair<bool, std::string> initialize_program_options() override;
    virtual std::pair<appbase::initialization_result::result, bool> initialize( int argc, char** argv ) = 0;
    virtual appbase::initialization_result::result start() = 0;

  public:

    beekeeper_app_init();
    ~beekeeper_app_init() override;
};

}
