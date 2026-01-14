
#include <hive/chain/hive_fwd.hpp>

#include <hive/plugins/metadata/metadata_plugin.hpp>

#include <hive/chain/database.hpp>
#include <hive/chain/index.hpp>
#include <hive/chain/account_object.hpp>
#include <chainbase/chainbase.inl>
#include <hive/chain/util/type_registrar_definition.hpp>

#include <hive/utilities/signal.hpp>

#include <fc/io/json.hpp>
#include <fc/exception/exception.hpp>

namespace hive { namespace plugins { namespace metadata {

namespace detail {

class metadata_plugin_impl
{
  private:

    metadata_plugin& _self;
    chain::database::signal_connection_ptr _on_metadata_conn;

  public:

    chain::database& _db;

    metadata_plugin_impl( metadata_plugin& self, appbase::application& app ):
      _self(self),
      _db( app.get_plugin< hive::plugins::chain::chain_plugin >().db() )
      {
        _on_metadata_conn = _db.add_metadata_handler( [&]( const hive::chain::metadata_notification& note )
        {
          on_metadata( note );
        }, _self );
      }
    virtual ~metadata_plugin_impl() = default;

    void on_metadata( const hive::chain::metadata_notification& note )
    {
      switch( note.action )
      {
        case hive::chain::metadata_action::account_create:
          _db.create< account_metadata_object >( [&]( account_metadata_object& meta )
          {
            meta.account = note.account;
            from_string( meta.json_metadata, note.json_metadata );
          });
          break;
        case hive::chain::metadata_action::account_create_with_delegation:
          _db.create< account_metadata_object >( [&]( account_metadata_object& meta )
          {
            meta.account = note.account;
            from_string( meta.json_metadata, note.json_metadata );
          });
          break;
        case hive::chain::metadata_action::account_update:
          if( note.json_metadata.size() > 0 )
          {
            _db.modify( _db.get< account_metadata_object, by_account >( note.account ), [&]( account_metadata_object& meta )
            {
              from_string( meta.json_metadata, note.json_metadata );
              if ( !_db.has_hardfork( HIVE_HARDFORK_0_21__3274 ) )
              {
                from_string( meta.posting_json_metadata, note.json_metadata );
              }
            });
          }
          break;
        case hive::chain::metadata_action::account_update2:
          if( note.json_metadata.size() > 0 || note.posting_json_metadata.size() > 0 )
          {
            _db.modify( _db.get< account_metadata_object, by_account >( note.account ), [&]( account_metadata_object& meta )
            {
              if ( note.json_metadata.size() > 0 )
                from_string( meta.json_metadata, note.json_metadata );

              if ( note.posting_json_metadata.size() > 0 )
                from_string( meta.posting_json_metadata, note.posting_json_metadata );
            });
          }
          break;
        case hive::chain::metadata_action::pow:
          _db.create< account_metadata_object >( [&]( account_metadata_object& meta )
          {
            meta.account = note.account;
          });
          break;
        case hive::chain::metadata_action::pow2:
          _db.create< account_metadata_object >( [&]( account_metadata_object& meta )
          {
            meta.account = note.account;
          });
          break;
        case hive::chain::metadata_action::create_claimed_account:
          _db.create< account_metadata_object >( [&]( account_metadata_object& meta )
          {
            meta.account = note.account;
            from_string( meta.json_metadata, note.json_metadata );
          });
          break;
        default:
          FC_ASSERT( false && "Unknown metadata action" );
          break;
      }
    }
};

} // detail

metadata_plugin::metadata_plugin() {}
metadata_plugin::~metadata_plugin() {}

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

    HIVE_ADD_PLUGIN_INDEX(my->_db, account_metadata_index);

    ilog( "metadata: plugin_initialize() end" );
  } FC_CAPTURE_AND_RETHROW()
}

void metadata_plugin::plugin_startup() {}

void metadata_plugin::plugin_shutdown()
{
}

} } } // hive::plugins::metadata

HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::plugins::metadata::account_metadata_index)