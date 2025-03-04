#pragma once

#include "fc/exception/exception.hpp"
#include <chainbase/dumper_custom_data.hpp>

#include <hive/plugins/state_snapshot/core.hpp>


namespace hive {

  class dumper_comment_data: public chainbase::dumper_custom_data
  {
    private:

      std::shared_ptr<xxx::core> c;

    public:

      dumper_comment_data();
      void write( const void* obj ) override;
  };
}