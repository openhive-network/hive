#pragma once

#include <core/beekeeper_app_init.hpp>

namespace beekeeper {

class beekeeper_wasm_app: public beekeeper_app_init
{
  private:

    hive::utilities::notifications::notification_handler_wrapper notification_handler;

    boost::program_options::variables_map args;

  protected:

    std::pair<bool, bool> initialize( int argc, char** argv ) override;
    bool start() override;

    const boost::program_options::variables_map& get_args() const override;
    bfs::path get_data_dir() const override;
    void setup_notifications( const boost::program_options::variables_map& args ) override;

  public:

    beekeeper_wasm_app();
    ~beekeeper_wasm_app() override;
};

}