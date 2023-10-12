#pragma once

#include "database_fixture.hpp"

#include <fc/log/logger_config.hpp>

namespace hive { namespace chain {

/** @brief Fixture mimicking hived start/initialization as close as possible.
 *  Also provides ability to simulate custom config.ini lines, therefore unlike other fixtures
 *  this one performs initialization in separate function (postponed_init).
 *  @note Initialization of required plugins is also ordered by providing appropriate 
 *  config-line parameters of postponed_init.
 *  You can either use this fixture directly and call postponed_init in each test (see config_tests)
 *  which allows providing different config.ini lines in each test.
 *  Alternatively you can subclass this fixture and call postponed_init only once, in subclass'
 *  constructor, providing same config.ini lines to each test.
 */
struct hived_fixture : public database_fixture
{
  private:
  
  /// @brief Alows verification that logging-related configuration is correctly processed.
  fc::optional< fc::logging_config > _logging_config;
  /// @brief Where tested "hived" data go. Valid after a call to postponed_init.
  fc::path _data_dir;

  public:

  hived_fixture();
  virtual ~hived_fixture();

  typedef std::vector< std::pair< std::string, std::vector< std::string > > > config_arg_override_t;
  typedef config_arg_override_t::value_type config_line_t;
  /** 
   * @brief Performs hived-like start/initialization of the node.
   * @note  Must be called before or at the beginning of the test.
   * @param config_arg_overrides Configuration lines as if they were found in config.ini.
   * @param plugin_ptr Pointers to be filled on output, allowing direct access to necessary plugins.
   */
  template< typename... Plugin >
  void postponed_init( const config_arg_override_t& config_arg_overrides = config_arg_override_t(), Plugin**... plugin_ptr )
  {
    postponed_init_impl( config_arg_overrides );
    // If any plugin pointer is required ...
    if constexpr ( sizeof...( plugin_ptr ) > 0 )
    {
      // ... use fold expression on parameter pack to fill each provided plugin pointer.
      ((*plugin_ptr = &theApp.get_plugin< Plugin >()), ...);
    }
  }

  const fc::optional< fc::logging_config > get_logging_config() const { return _logging_config; }
  const fc::path& get_data_dir() const { return _data_dir; };

  const hive::chain::block_read_i& get_block_reader() const;

  private:
    void postponed_init_impl( const config_arg_override_t& config_arg_overrides );
  private:
    const hive::chain::block_read_i* _block_reader = nullptr;
};

struct json_rpc_database_fixture : public hived_fixture
{
  private:
    hive::plugins::json_rpc::json_rpc_plugin* rpc_plugin;

    fc::variant get_answer( std::string& request );
    void review_answer( fc::variant& answer, int64_t code, bool is_warning, bool is_fail, fc::optional< fc::variant > id,
      const char* message = nullptr );

  public:

    json_rpc_database_fixture();
    virtual ~json_rpc_database_fixture();

    void make_array_request( std::string& request, int64_t code = 0, bool is_warning = false, bool is_fail = true );
    fc::variant make_request( std::string& request, int64_t code = 0, bool is_warning = false, bool is_fail = true,
      const char* message = nullptr );
    void make_positive_request( std::string& request );
};

} }
