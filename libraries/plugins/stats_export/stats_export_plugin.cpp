
#include <hive/plugins/block_data_export/block_data_export_plugin.hpp>
#include <hive/plugins/block_data_export/exportable_block_data.hpp>

#include <hive/plugins/stats_export/stats_export_plugin.hpp>

#include <hive/chain/account_object.hpp>
#include <hive/chain/database.hpp>
#include <hive/chain/index.hpp>

#include <hive/plugins/database_api/database_api_objects.hpp>

#include <fstream>
#include <iostream>
#include <sstream>

namespace hive { namespace plugins { namespace stats_export {

using hive::chain::block_notification;
using hive::chain::database;
using hive::plugins::database_api::api_dynamic_global_property_object;

using hive::protocol::account_name_type;
using hive::protocol::authority;
using hive::protocol::signed_transaction;

using hive::plugins::block_data_export::block_data_export_plugin;
using hive::plugins::block_data_export::exportable_block_data;

using hive::plugins::chain::chain_plugin;

namespace detail {

struct api_stats_transaction_data_object
{
  account_name_type     user;
  uint32_t              size = 0;
};

class api_stats_export_data_object
  : public exportable_block_data
{
  public:
    api_stats_export_data_object() {}
    virtual ~api_stats_export_data_object() {}

    virtual void to_variant( fc::variant& v )const override
    {
      fc::to_variant( *this, v );
    }

    api_dynamic_global_property_object                    global_properties;
    std::vector< api_stats_transaction_data_object >      transaction_stats;
    uint64_t                                              free_memory = 0;
};

} } } }

FC_REFLECT( hive::plugins::stats_export::detail::api_stats_transaction_data_object, (user)(size) )
FC_REFLECT( hive::plugins::stats_export::detail::api_stats_export_data_object, (global_properties)(transaction_stats)(free_memory) )

namespace hive { namespace plugins { namespace stats_export { namespace detail {

class stats_export_plugin_impl
{
  public:
    stats_export_plugin_impl( stats_export_plugin& _plugin, appbase::application& app ) :
      _db( app.get_plugin< chain_plugin >().db() ),
      _self( _plugin ),
      _export_plugin( app.get_plugin< block_data_export_plugin >() )
      {}

    void on_post_apply_block( const block_notification& note );

    database&                     _db;
    stats_export_plugin&          _self;
    boost::signals2::connection   _post_apply_block_conn;

    block_data_export_plugin&     _export_plugin;
};

account_name_type get_transaction_user( const signed_transaction& tx )
{
  flat_set< account_name_type > active;
  flat_set< account_name_type > owner;
  flat_set< account_name_type > posting;
  flat_set< account_name_type > witness;
  vector< authority > other;

  tx.get_required_authorities( active, owner, posting, witness, other );

  for( const account_name_type& name : posting )
    return name;
  for( const account_name_type& name : active )
    return name;
  for( const account_name_type& name : owner )
    return name;
  for( const account_name_type& name : witness )
    return name;
  return account_name_type();
}

void stats_export_plugin_impl::on_post_apply_block( const block_notification& note )
{
  std::shared_ptr< api_stats_export_data_object > stats = _export_plugin.find_export_data< api_stats_export_data_object >( HIVE_STATS_EXPORT_PLUGIN_NAME );
  if( !stats )
    return;

  stats->global_properties = api_dynamic_global_property_object( _db.get_dynamic_global_properties(), _db );
  for( const auto& tx : note.full_block->get_full_transactions() )
  {
    stats->transaction_stats.emplace_back();

    api_stats_transaction_data_object& tx_stats = stats->transaction_stats.back();
    tx_stats.user = get_transaction_user( tx->get_transaction() );
    tx_stats.size = tx->get_transaction_size();
  }

  stats->free_memory = _db.get_free_memory();
}

} // detail

stats_export_plugin::stats_export_plugin( appbase::application& app ): appbase::plugin<stats_export_plugin>( app ) {}
stats_export_plugin::~stats_export_plugin() {}

void stats_export_plugin::set_program_options( options_description& cli, options_description& cfg )
{
  /*
  cfg.add_options()
      ("block-log-info-print-interval-seconds", boost::program_options::value< int32_t >()->default_value(60*60*24), "How often to print out stats_export (default 1 day)")
      ("block-log-info-print-irreversible", boost::program_options::value< bool >()->default_value(true), "Whether to defer printing until block is irreversible")
      ("block-log-info-print-file", boost::program_options::value< string >()->default_value("ILOG"), "Where to print (filename or special sink ILOG, STDOUT, STDERR)")
      ;
  */
}

void stats_export_plugin::plugin_initialize( const boost::program_options::variables_map& options )
{
  my = std::make_unique< detail::stats_export_plugin_impl >( *this, theApp );
  try
  {
    ilog( "Initializing stats_export plugin" );
    my->_post_apply_block_conn = my->_db.add_post_apply_block_handler(
      [&]( const block_notification& note ){ my->on_post_apply_block( note ); }, *this );
    my->_export_plugin.register_export_data_factory( HIVE_STATS_EXPORT_PLUGIN_NAME,
      []() -> std::shared_ptr< exportable_block_data > { return std::make_shared< detail::api_stats_export_data_object >(); } );
  }
  FC_CAPTURE_AND_RETHROW()
}

void stats_export_plugin::plugin_startup() {}

void stats_export_plugin::plugin_shutdown()
{
  chain::util::disconnect_signal( my->_post_apply_block_conn );
}

} } } // hive::plugins::stats_export
