#pragma once

#include <core/beekeeper_app_base.hpp>

namespace beekeeper {

class beekeeper_wasm_app: public beekeeper_app_base
{
  private:

    bool enable_logs = true;
    fc::log_level old_log_level = fc::log_level::off;

    boost::program_options::variables_map args;

  protected:

    void set_program_options() override;
    uint32_t initialize( int argc, char** argv ) override;
    void start() override;

    const boost::program_options::variables_map& get_args() const override;
    bfs::path get_data_dir() const override;
    void setup( const boost::program_options::variables_map& args ) override;

    std::shared_ptr<beekeeper::beekeeper_wallet_manager> create_wallet( const boost::filesystem::path& cmd_wallet_dir, uint64_t cmd_unlock_timeout, uint32_t cmd_session_limit ) override;

  public:

    beekeeper_wasm_app();
    ~beekeeper_wasm_app() override;

    uint32_t init( int argc, char** argv ) override;
};

}