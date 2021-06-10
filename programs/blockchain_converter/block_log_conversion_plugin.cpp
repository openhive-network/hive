#include "block_log_conversion_plugin.hpp"

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

namespace converter { namespace plugins { namespace block_log_conversion {

namespace detail {

  class block_log_conversion_plugin_impl final : public conversion_plugin_impl {
  public:
    block_log log_in, log_out;

    block_log_conversion_plugin_impl( const private_key_type& _private_key, const chain_id_type& chain_id = HIVE_CHAIN_ID )
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
    FC_ASSERT( log_in.is_open() && log_out.is_open(), "Block logs should be opened before conversion" );
    FC_ASSERT( log_in.head(), "Your input block log is empty" );

    if( !start_block_num )
      start_block_num = log_out.head() ? log_out.read_head().block_num() : 1;

    if( !stop_block_num || stop_block_num > log_in.head()->block_num() )
      stop_block_num = log_in.head()->block_num();

    block_id_type last_block_id{};

    for( ; start_block_num <= stop_block_num && !appbase::app().is_interrupt_request(); ++start_block_num )
    {
      fc::optional< signed_block > block = log_in.read_block_by_num( start_block_num );
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
    log_in.close();
    log_out.close();
    if( block_header::num_from_id( converter.get_previous_block_id() ) + 1 <= HIVE_HARDFORK_0_17_BLOCK_NUM )
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

    my = std::make_unique< detail::block_log_conversion_plugin_impl >( *private_key, _hive_chain_id );

    my->log_per_block = options["log-per-block"].as< uint32_t >();
    my->log_specific = options["log-specific"].as< uint32_t >();

    try
    {
      my->open( block_log_in, block_log_out );
    } FC_CAPTURE_AND_RETHROW()

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
  }

  void block_log_conversion_plugin::plugin_startup() {
    my->convert(
      appbase::app().get_args().at("resume-block").as< uint32_t >(),
      appbase::app().get_args().at( "stop-block" ).as< uint32_t >()
    );
  }
  void block_log_conversion_plugin::plugin_shutdown()
  {
    my->close();
    my->print_wifs();
  }

} } } } // hive::converter::plugins::block_log_conversion