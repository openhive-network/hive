
#include <hive/chain/hive_fwd.hpp>

#include <hive/plugins/tags/tags_plugin.hpp>

#include <hive/protocol/config.hpp>

#include <hive/chain/database.hpp>
#include <hive/chain/index.hpp>
#include <hive/chain/account_object.hpp>
#include <hive/chain/comment_object.hpp>

#include <fc/smart_ref_impl.hpp>
#include <fc/thread/thread.hpp>
#include <fc/io/json.hpp>
#include <fc/string.hpp>

#include <boost/range/iterator_range.hpp>
#include <boost/algorithm/string.hpp>

namespace hive { namespace plugins { namespace tags {

namespace detail {

using namespace hive::protocol;

class tags_plugin_impl
{
  public:
    tags_plugin_impl();
    virtual ~tags_plugin_impl();

    chain::database&     _db;
    fc::time_point_sec   _promoted_start_time;
    bool                 _started = false;

};

tags_plugin_impl::tags_plugin_impl() :
  _db( appbase::app().get_plugin< hive::plugins::chain::chain_plugin >().db() ) {}

tags_plugin_impl::~tags_plugin_impl() {}


} /// end detail namespace

tags_plugin::tags_plugin() {}
tags_plugin::~tags_plugin() {}

void tags_plugin::set_program_options(
  boost::program_options::options_description& cli,
  boost::program_options::options_description& cfg
  )
{
  cfg.add_options()
    ("tags-start-promoted", boost::program_options::value< uint32_t >()->default_value( 0 ), "Block time (in epoch seconds) when to start calculating promoted content. Should be 1 week prior to current time." )
    ("tags-skip-startup-update", bpo::value<bool>()->default_value(false), "Skip updating tags on startup. Can safely be skipped when starting a previously running node. Should not be skipped when reindexing.")
    ;
  cli.add_options()
    ("tags-skip-startup-update", bpo::bool_switch()->default_value(false), "Skip updating tags on startup. Can safely be skipped when starting a previously running node. Should not be skipped when reindexing." )
    ;
}

void tags_plugin::plugin_initialize(const boost::program_options::variables_map& options)
{
  ilog("Intializing tags plugin" );
  my = std::make_unique< detail::tags_plugin_impl >();

  fc::variant_object_builder state_opts;

  if( options.count( "tags-start-promoted" ) )
  {
    my->_promoted_start_time = fc::time_point_sec( options[ "tags-start-promoted" ].as< uint32_t >() );
    state_opts("tags-start-promoted", my->_promoted_start_time);
    idump( (my->_promoted_start_time) );
  }

  appbase::app().get_plugin< chain::chain_plugin >().report_state_options( name(), state_opts.get() );
}


void tags_plugin::plugin_startup()
{
  my->_started = true;
}

void tags_plugin::plugin_shutdown()
{
}

} } } /// hive::plugins::tags
