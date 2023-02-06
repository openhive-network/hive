#pragma once

#include <appbase/application.hpp>

#include <boost/program_options.hpp>

#include <memory>
#include <string>

#define HIVE_ICEBERG_GENERATE_CONVERSION_PLUGIN_NAME "iceberg_generate"

namespace hive { namespace converter { namespace plugins { namespace iceberg_generate {

  namespace bpo = boost::program_options;

  namespace detail {
    class iceberg_generate_plugin_impl;
  }

  class iceberg_generate_plugin final : public appbase::plugin< iceberg_generate_plugin > {
  public:
    APPBASE_PLUGIN_REQUIRES()

    iceberg_generate_plugin();
    virtual ~iceberg_generate_plugin();

    static const std::string& name()
    {
      static std::string name = HIVE_ICEBERG_GENERATE_CONVERSION_PLUGIN_NAME;
      return name;
    }

    virtual void set_program_options( bpo::options_description& cli, bpo::options_description& cfg ) override;
    virtual void plugin_initialize( const bpo::variables_map& options ) override;
    virtual void plugin_startup() override;
    virtual void plugin_shutdown() override;

  private:
    std::unique_ptr< detail::iceberg_generate_plugin_impl > my;
  };

}}}} // namespace hive::converter::plugins::iceberg_generate
