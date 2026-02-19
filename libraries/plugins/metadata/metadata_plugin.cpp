
#include <hive/chain/hive_fwd.hpp>

#include <hive/plugins/metadata/metadata_plugin.hpp>

#include <hive/chain/database.hpp>
#include <hive/chain/index.hpp>
#include <hive/chain/account_object.hpp>
#include <hive/chain/account_object_multiindex.hpp>
#include <hive/chain/notifications.hpp>

#include <hive/utilities/signal.hpp>

#include <hive/protocol/hive_operations.hpp>

#include <fc/io/json.hpp>
#include <fc/exception/exception.hpp>

namespace hive { namespace plugins { namespace metadata {

// Use types from chain namespace
using hive::chain::account_metadata_object;
using hive::chain::by_account;

namespace detail {

using namespace hive::protocol;

class metadata_plugin_impl
{
  public:

    metadata_plugin& _self;
    chain::database& _db;
    chain::database::signal_connection_ptr _post_apply_operation_conn;

    metadata_plugin_impl( metadata_plugin& self, appbase::application& app ):
      _self(self),
      _db( app.get_plugin< hive::plugins::chain::chain_plugin >().db() )
    {
    }

    ~metadata_plugin_impl() = default;

    void on_post_apply_operation( const hive::chain::operation_notification& note );
};

struct pow2_work_get_account_visitor
{
  typedef const account_name_type* result_type;

  template< typename WorkType >
  result_type operator()( const WorkType& work )const
  {
    return &work.input.worker_account;
  }
};

struct post_operation_visitor
{
  chain::database& db;

  post_operation_visitor( metadata_plugin_impl& plugin ) : db( plugin._db ) {}

  typedef void result_type;

  template< typename T >
  void operator()( const T& )const {}

  void operator()( const account_create_operation& op )const
  {
    const auto& new_account = db.get_account( op.new_account_name );
    db.create< account_metadata_object >( [&]( account_metadata_object& meta )
    {
      meta.account = new_account.get_id();
      from_string( meta.json_metadata, op.json_metadata );
    });
  }

  void operator()( const account_create_with_delegation_operation& op )const
  {
    const auto& new_account = db.get_account( op.new_account_name );
    db.create< account_metadata_object >( [&]( account_metadata_object& meta )
    {
      meta.account = new_account.get_id();
      from_string( meta.json_metadata, op.json_metadata );
    });
  }

  void operator()( const create_claimed_account_operation& op )const
  {
    const auto& new_account = db.get_account( op.new_account_name );
    db.create< account_metadata_object >( [&]( account_metadata_object& meta )
    {
      meta.account = new_account.get_id();
      from_string( meta.json_metadata, op.json_metadata );
    });
  }

  void operator()( const pow_operation& op )const
  {
    const auto& account = db.get_account( op.worker_account );
    const auto* existing_meta = db.find< account_metadata_object, by_account >( account.get_id() );
    if( existing_meta != nullptr )
      return;
    db.create< account_metadata_object >( [&]( account_metadata_object& meta )
    {
      meta.account = account.get_id();
    });
  }

  void operator()( const pow2_operation& op )const
  {
    const account_name_type* worker_account = op.work.visit( pow2_work_get_account_visitor() );
    const auto& account = db.get_account( *worker_account );
    const auto* existing_meta = db.find< account_metadata_object, by_account >( account.get_id() );
    if( existing_meta != nullptr )
      return;
    db.create< account_metadata_object >( [&]( account_metadata_object& meta )
    {
      meta.account = account.get_id();
    });
  }

  void operator()( const account_update_operation& op )const
  {
    if( op.json_metadata.size() == 0 )
      return;

    const auto& account = db.get_account( op.account );
    db.modify( db.get< account_metadata_object, by_account >( account.get_id() ), [&]( account_metadata_object& meta )
    {
      from_string( meta.json_metadata, op.json_metadata );
      if( !db.has_hardfork( HIVE_HARDFORK_0_21__3274 ) )
      {
        from_string( meta.posting_json_metadata, op.json_metadata );
      }
    });
  }

  void operator()( const account_update2_operation& op )const
  {
    if( op.json_metadata.size() == 0 && op.posting_json_metadata.size() == 0 )
      return;

    const auto& account = db.get_account( op.account );
    db.modify( db.get< account_metadata_object, by_account >( account.get_id() ), [&]( account_metadata_object& meta )
    {
      if( op.json_metadata.size() > 0 )
        from_string( meta.json_metadata, op.json_metadata );

      if( op.posting_json_metadata.size() > 0 )
        from_string( meta.posting_json_metadata, op.posting_json_metadata );
    });
  }
};

void metadata_plugin_impl::on_post_apply_operation( const hive::chain::operation_notification& note )
{
  note.op.visit( post_operation_visitor( *this ) );
}

} // detail

metadata_plugin::metadata_plugin() {}
metadata_plugin::~metadata_plugin() {}

get_account_metadata_return metadata_plugin::get_account_metadata( const account_object& account ) const
{
  get_account_metadata_return result;

  const auto* meta = my->_db.find< account_metadata_object, by_account >( account.get_id() );
  if( meta == nullptr )
    return result;

  result.json_metadata = to_string( meta->json_metadata );
  result.posting_json_metadata = to_string( meta->posting_json_metadata );

  return result;
}

void metadata_plugin::set_program_options(
  boost::program_options::options_description& cli,
  boost::program_options::options_description& cfg
)
{
}

void metadata_plugin::plugin_initialize( const boost::program_options::variables_map& options )
{
  try
  {
    ilog( "metadata: plugin_initialize() begin" );
    my = std::make_unique< detail::metadata_plugin_impl >( *this, get_app() );

    chain::database& db = my->_db;

    my->_post_apply_operation_conn = db.add_post_apply_operation_handler(
      [&]( const hive::chain::operation_notification& note ){ my->on_post_apply_operation( note ); }, *this, 0 );

    // Note: account_metadata_index is now registered as a core index in database_authority.cpp
    // so we don't need to add it as a plugin index here.

    ilog( "metadata: plugin_initialize() end" );
  } FC_CAPTURE_AND_RETHROW()
}

void metadata_plugin::plugin_startup() {}

void metadata_plugin::plugin_shutdown()
{
  hive::utilities::disconnect_signal( my->_post_apply_operation_conn );
}

} } } // hive::plugins::metadata

// Note: Type registration for account_metadata_index is now done in
// database_authority.cpp as it's a core index, not a plugin index.
