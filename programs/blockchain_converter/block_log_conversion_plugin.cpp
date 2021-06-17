#include "block_log_conversion_plugin.hpp"

#include <appbase/application.hpp>

#include <fc/exception/exception.hpp>
#include <fc/optional.hpp>
#include <fc/filesystem.hpp>
#include <fc/io/json.hpp>
#include <fc/log/logger.hpp>
#include <fc/variant.hpp>

#include <hive/chain/block_log.hpp>

#include <hive/protocol/config.hpp>
#include <hive/protocol/types.hpp>
#include <hive/protocol/block.hpp>

#include <hive/utilities/key_conversion.hpp>

#include <boost/program_options.hpp>

#include <string>
#include <iostream>
#include <memory>

#include "conversion_plugin.hpp"

#include "converter.hpp"

namespace hive {namespace converter { namespace plugins { namespace block_log_conversion {

  namespace bpo = boost::program_options;

  namespace hp = hive::protocol;

  using hive::utilities::wif_to_key;

namespace detail {

  using hive::chain::block_log;

  class block_log_conversion_plugin_impl final : public conversion_plugin_impl {
  public:
    block_log log_in, log_out;

    block_log_conversion_plugin_impl( const hp::private_key_type& _private_key, const hp::chain_id_type& chain_id = HIVE_CHAIN_ID )
      : conversion_plugin_impl( _private_key, chain_id ) {}

    virtual void convert( uint32_t start_block_num, uint32_t stop_block_num ) override;
    void open( const fc::path& input, const fc::path& output );
    void close();
  };

  void block_log_conversion_plugin_impl::open( const fc::path& input, const fc::path& output )
  {
    try
    {
      log_in.open( input );
    } FC_CAPTURE_AND_RETHROW( (input) )

    try
    {
      log_out.open( output );
    } FC_CAPTURE_AND_RETHROW( (output) )
  }

  void block_log_conversion_plugin_impl::convert( uint32_t start_block_num, uint32_t stop_block_num )
  {
    FC_ASSERT( log_in.is_open(), "Input block log should be opened before the conversion" );
    FC_ASSERT( log_out.is_open(), "Output block log should be opened before the conversion" );
    FC_ASSERT( log_in.head(), "Your input block log is empty" );

    if( !start_block_num )
      start_block_num = log_out.head() ? log_out.read_head().block_num() : 1;

    if( !stop_block_num || stop_block_num > log_in.head()->block_num() )
      stop_block_num = log_in.head()->block_num();

    hp::block_id_type last_block_id;

    for( ; start_block_num <= stop_block_num && !appbase::app().is_interrupt_request(); ++start_block_num )
    {
      fc::optional< hp::signed_block > block = log_in.read_block_by_num( start_block_num );
      FC_ASSERT( block.valid(), "unable to read block" );

      if ( ( log_per_block > 0 && start_block_num % log_per_block == 0 ) || log_specific == start_block_num )
      {
        fc::json json_block;
        fc::variant v;
        fc::to_variant( *block, v );

        std::cout << "Rewritten block: " << start_block_num
          << ". Data before conversion: " << json_block.to_string( v ) << '\n';
      }

      last_block_id = converter.convert_signed_block( *block, last_block_id );

      if( start_block_num % 1000 == 0 ) // Progress
      {
        std::cout << "[ " << int( float(start_block_num) / log_in.head()->block_num() * 100 ) << "% ]: " << start_block_num << '/' << log_in.head()->block_num() << " blocks rewritten.\r";
        std::cout.flush();
      }

      log_out.append( *block );

      if ( ( log_per_block > 0 && start_block_num % log_per_block == 0 ) || log_specific == start_block_num )
      {
        fc::json json_block;
        fc::variant v;
        fc::to_variant( *block, v );

        std::cout << "After conversion: " << json_block.to_string( v ) << '\n';
      }
    }

    if( !appbase::app().is_interrupt_request() )
      appbase::app().generate_interrupt_request();
  }

  void block_log_conversion_plugin_impl::close()
  {
    if( log_in.is_open() )
      log_in.close();

    if( log_out.is_open() )
      log_out.close();

    if( hp::block_header::num_from_id( converter.get_previous_block_id() ) + 1 <= HIVE_HARDFORK_0_17_BLOCK_NUM )
      std::cerr << "Second authority has not been applied on the accounts yet! Try resuming the conversion process\n";
  }

} // detail

  block_log_conversion_plugin::block_log_conversion_plugin() {}
  block_log_conversion_plugin::~block_log_conversion_plugin() {}

  void block_log_conversion_plugin::set_program_options( bpo::options_description& cli, bpo::options_description& cfg ) {}

  void block_log_conversion_plugin::plugin_initialize( const bpo::variables_map& options )
  {
    FC_ASSERT( options.count("input"), "You have to specify the input source for the " HIVE_BLOCK_LOG_CONVERSION_PLUGIN_NAME " plugin" );

    std::string out_file;
    if( !options.count("output") )
      out_file = options["input"].as< std::string >() + "_out";
    else
      out_file = options["output"].as< std::string >();

    fc::path block_log_in( options["input"].as< std::string >() );
    fc::path block_log_out( out_file );

    hp::chain_id_type _hive_chain_id;

    const auto& chain_id_str = options["chain-id"].as< std::string >();

    try
    {
      _hive_chain_id = hp::chain_id_type( chain_id_str );
    }
    catch( fc::exception& )
    {
      FC_ASSERT( false, "Could not parse chain_id as hex string. Chain ID String: ${s}", ("s", chain_id_str) );
    }

    const auto private_key = wif_to_key( options["private-key"].as< std::string >() );
    FC_ASSERT( private_key.valid(), "unable to parse the private key" );

    my = std::make_unique< detail::block_log_conversion_plugin_impl >( *private_key, _hive_chain_id );

    my->set_logging( options["log-per-block"].as< uint32_t >(), options["log-specific"].as< uint32_t >() );

    my->set_wifs( options.count("use-same-key"), options["owner-key"].as< std::string >(), options["active-key"].as< std::string >(), options["posting-key"].as< std::string >() );

    my->open( block_log_in, block_log_out );
  }

  void block_log_conversion_plugin::plugin_startup() {
    try
    {
      my->convert(
        appbase::app().get_args().at("resume-block").as< uint32_t >(),
        appbase::app().get_args().at( "stop-block" ).as< uint32_t >()
      );
    }
    catch( const fc::exception& e )
    {
      elog( e.to_detail_string() );
      appbase::app().generate_interrupt_request();
    }
  }
  void block_log_conversion_plugin::plugin_shutdown()
  {
    my->close();
    my->print_wifs();
  }

} } } } // hive::converter::plugins::block_log_conversion
