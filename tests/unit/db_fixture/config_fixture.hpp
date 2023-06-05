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

  public:

  config_fixture();
  virtual ~config_fixture();

  const fc::optional< fc::logging_config > get_logging_config() const { return _logging_config; }
};

} }
