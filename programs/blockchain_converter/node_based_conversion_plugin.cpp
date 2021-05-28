#include "node_based_conversion_plugin.hpp"

#include <appbase/application.hpp>

#include <fc/exception/exception.hpp>
#include <fc/optional.hpp>
#include <fc/filesystem.hpp>
#include <fc/io/json.hpp>
#include <fc/variant.hpp>

#include <hive/chain/block_log.hpp>

#include <hive/protocol/block.hpp>

#include <hive/utilities/key_conversion.hpp>

#include <boost/program_options.hpp>

#include <memory>

#include "conversion_plugin.hpp"

#include "converter.hpp"

namespace bpo = boost::program_options;

namespace hive {

using namespace chain;
using namespace utilities;
using namespace protocol;

namespace converter { namespace plugins { namespace node_based_conversion {

namespace detail {

  class node_based_conversion_plugin_impl final : public conversion_plugin_impl {
  public:

    node_based_conversion_plugin_impl( const private_key_type& _private_key, const chain_id_type& chain_id = HIVE_CHAIN_ID )
      : conversion_plugin_impl( _private_key, chain_id ) {}

    virtual void convert( uint32_t start_block_num, uint32_t stop_block_num ) override;
  };

  void node_based_conversion_plugin_impl::convert( uint32_t start_block_num, uint32_t stop_block_num )
  {

  }

} // detail

  node_based_conversion_plugin::node_based_conversion_plugin() {}
  node_based_conversion_plugin::~node_based_conversion_plugin() {}

  void node_based_conversion_plugin::set_program_options( bpo::options_description& cli, bpo::options_description& cfg )
  {}

  void node_based_conversion_plugin::plugin_initialize( const bpo::variables_map& options )
  {
    chain_id_type _hive_chain_id;

    auto chain_id_str = options["chain-id"].as< std::string >();

    try
    {
      _hive_chain_id = chain_id_type( chain_id_str );
    }
    catch( fc::exception& )
    {
      FC_ASSERT( false, "Could not parse chain_id as hex string. Chain ID String: ${s}", ("s", chain_id_str) );
    }

    fc::optional< private_key_type > private_key = wif_to_key( options["private-key"].as< std::string >() );
    FC_ASSERT( private_key.valid(), "unable to parse the private key" );

    my = std::make_unique< detail::node_based_conversion_plugin_impl >( *private_key, _hive_chain_id );

    my->log_per_block = options["log-per-block"].as< uint32_t >();
    my->log_specific = options["log-specific"].as< uint32_t >();

    private_key_type owner_key;
    private_key_type active_key;
    private_key_type posting_key;
    fc::optional< private_key_type > _owner_key = wif_to_key( options.count("owner-key") ? options["owner-key"].as< std::string >() : "" );
    fc::optional< private_key_type > _active_key = wif_to_key( options.count("active-key") ? options["active-key"].as< std::string >() : "" );
    fc::optional< private_key_type > _posting_key = wif_to_key( options.count("posting-key") ? options["posting-key"].as< std::string >() : "" );

    if( _owner_key.valid() )
      owner_key = *_owner_key;
    else if( options.count("use-same-key") )
      owner_key = *private_key;
    else
      owner_key = private_key_type::generate();

    if( _active_key.valid() )
      active_key = *_active_key;
    else if( options.count("use-same-key") )
      active_key = *private_key;
    else
      active_key = private_key_type::generate();

    if( _posting_key.valid() )
      posting_key = *_posting_key;
    else if( options.count("use-same-key") )
      posting_key = *private_key;
    else
      posting_key = private_key_type::generate();

    my->converter.set_second_authority_key( owner_key, authority::owner );
    my->converter.set_second_authority_key( active_key, authority::active );
    my->converter.set_second_authority_key( posting_key, authority::posting );

    stop_block_num = options.at("stop-at-block").as< uint32_t >();
  }

  void node_based_conversion_plugin::plugin_startup() {
    my->convert( 0, stop_block_num );
  }
  void node_based_conversion_plugin::plugin_shutdown()
  {
    my->print_wifs();
  }

} } } } // hive::converter::plugins::block_log_conversion