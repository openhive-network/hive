
#include <hive/chain/hive_fwd.hpp>

#include <hive/protocol/config.hpp>
#include <hive/chain/rc/rc_curve.hpp>
#include <hive/chain/rc/rc_export_objects.hpp>
#include <hive/plugins/rc/rc_plugin.hpp>
#include <hive/chain/rc/rc_objects.hpp>
#include <hive/chain/rc/rc_operations.hpp>

#include <hive/chain/rc/resource_count.hpp>
#include <hive/chain/rc/resource_user.hpp>

#include <hive/chain/account_object.hpp>
#include <hive/chain/database.hpp>
#include <hive/chain/database_exceptions.hpp>
#include <hive/chain/generic_custom_operation_interpreter.hpp>
#include <hive/chain/index.hpp>
#include <hive/chain/util/type_registrar_definition.hpp>

#include <hive/chain/util/remove_guard.hpp>

#include <hive/jsonball/jsonball.hpp>

#include <boost/algorithm/string.hpp>
#include <cmath>

#ifndef IS_TEST_NET
#define HIVE_HF20_BLOCK_NUM 26256743
#endif

namespace hive { namespace plugins { namespace rc {

using namespace hive::chain;

namespace detail {

using chain::plugin_exception;
using hive::chain::util::manabar_params;

class rc_plugin_impl
{
  public:
    rc_plugin_impl( rc_plugin& _plugin ) :
      _db( appbase::app().get_plugin< hive::plugins::chain::chain_plugin >().db() ),
      _self( _plugin )
    {}

    database&                     _db;
    rc_plugin&                    _self;
};

} // detail

rc_plugin::rc_plugin() {}
rc_plugin::~rc_plugin() {}

void rc_plugin::set_program_options( options_description& cli, options_description& cfg )
{
}

void rc_plugin::plugin_initialize( const boost::program_options::variables_map& options )
{
  ilog( "Initializing resource credit plugin" );

  my = std::make_unique< detail::rc_plugin_impl >( *this );
}

void rc_plugin::plugin_startup() {}

void rc_plugin::plugin_shutdown() {}

} } } // hive::plugins::rc
