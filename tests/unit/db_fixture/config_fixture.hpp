#pragma once

#include "database_fixture.hpp"

#include <fc/log/logger_config.hpp>

namespace hive { namespace chain {

/// @brief  Fixture supporting manipulation of config.ini file.
struct config_fixture : public database_fixture
{
  private:
  
  /// @brief Use to verify that logging-related configuration is correctly processed.
  fc::optional< fc::logging_config > _logging_config;
  /// @brief Where tested "hived" data go. Valid after a call to postponed_init.
  fc::path _data_dir;

  public:

  config_fixture();
  virtual ~config_fixture();

  /** The uniqueness of this fixture & its tests is that the test provide parameters
   *  for appbase initialization, thus forcing its postponement.
   */
  typedef std::vector< std::pair< std::string, std::vector< std::string > > > config_arg_override_t;
  void postponed_init( const config_arg_override_t& config_arg_overrides = config_arg_override_t() );

  const fc::optional< fc::logging_config > get_logging_config() const { return _logging_config; }
  const fc::path& get_data_dir() const { return _data_dir; };
};

} }
