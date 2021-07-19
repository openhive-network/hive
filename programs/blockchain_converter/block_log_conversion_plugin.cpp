#include "block_log_conversion_plugin.hpp"

#include <appbase/application.hpp>

#include <fc/exception/exception.hpp>
#include <fc/optional.hpp>
#include <fc/filesystem.hpp>
#include <fc/io/json.hpp>
#include <fc/log/logger.hpp>
#include <fc/variant.hpp>

#include <hive/chain/block_log.hpp>

#include <hive/protocol/authority.hpp>
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
  using hive::utilities::key_to_wif;

namespace detail {

  using hive::chain::block_log;

  class block_log_conversion_plugin_impl final : public conversion_plugin_impl {
  public:
    block_log log_in, log_out;

    block_log_conversion_plugin_impl( const hp::private_key_type& _private_key, const hp::chain_id_type& chain_id = HIVE_CHAIN_ID, size_t signers_size = 1 )
      : conversion_plugin_impl( _private_key, chain_id, signers_size ) {}

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

    // Automatically set start_block_num if not set
    if( !log_out.head() ) // If output block log does not exist start from the beginning
      start_block_num = 1;
    else if( !start_block_num ) // If output block log exists than continue
      start_block_num = log_out.head()->block_num() + 1;
    else
      FC_ASSERT( start_block_num == log_out.head()->block_num() + 1,
        "Resume block should be equal to the head block number of the output block log + 1", ("out_head_block",log_out.head()->block_num())("start_block_num",start_block_num) );

    hp::block_id_type last_block_id;

    if( start_block_num > 1 ) // continuing conversion
    {
      FC_ASSERT( start_block_num <= log_in.head()->block_num(), "cannot resume conversion from a block that is not in the block_log",
        ("start_block_num", start_block_num)("log_in_head_block_num", log_in.head()->block_num()) );
      std::cout << "Continuing conversion from the block with number " << start_block_num
                << "\nValidating the chain id...\n";

      last_block_id = log_out.head()->id(); // Required to resume the conversion

      // Validate the chain id on conversion resume (in the best-case scenario, the complexity of this check is nearly constant - when the last block in the output block log has transactions with signatures)
      bool chain_id_match = false;
      uint32_t it_block_num = log_out.head()->block_num();

      while( !chain_id_match && it_block_num >= 1 )
      {
        fc::optional< hp::signed_block > block = log_out.read_block_by_num( it_block_num );
        FC_ASSERT( block.valid(), "unable to read block", ("block_num", it_block_num) );

        if( block->transactions.size() )
          if( block->transactions.begin()->signatures.size() )
          {
            const auto& trx = *block->transactions.begin();
            const auto& sig = *block->transactions.begin()->signatures.begin();

            std::cout << "Comparing signatures in trx " << trx.id().str() << " in block: " << block->block_num() << ": \n"
                      << "Previous sig: " << std::hex;
            // Display the previous sig
            for( const auto c : sig )
              std::cout << uint32_t(c);
            std::cout << "\nCurrent sig:  ";

            const auto sig_other = converter.get_second_authority_key( hp::authority::owner ).sign_compact( trx.sig_digest( /* Current chain id: */ converter.get_chain_id() ) );

            // Display the current sig
            for( const auto c : sig_other )
              std::cout << uint32_t(c);
            std::cout << std::dec << std::endl;

            if( sig == sig_other )
              chain_id_match = true;
            else
              break;
          }

        if( it_block_num == 1 )
          chain_id_match = true; // No transactions in the entire block_log, so the chain id matches
        --it_block_num;
      }
      FC_ASSERT( chain_id_match, "Previous output block log chain id does not match the specified one or the owner key of the 2nd authority has changed",
        ("chain_id", converter.get_chain_id())("owner_key", key_to_wif(converter.get_second_authority_key( hp::authority::owner ))) );
    }

    if( !stop_block_num || stop_block_num > log_in.head()->block_num() )
      stop_block_num = log_in.head()->block_num();

    for( ; start_block_num <= stop_block_num && !appbase::app().is_interrupt_request(); ++start_block_num )
    {
      fc::optional< hp::signed_block > block = log_in.read_block_by_num( start_block_num );
      FC_ASSERT( block.valid(), "unable to read block", ("block_num", start_block_num) );

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
        std::cout << "[ " << int( float(start_block_num) / stop_block_num * 100 ) << "% ]: " << start_block_num << '/' << stop_block_num << " blocks rewritten.\r";
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
      std::cerr << "Conversion interrupted before HF17. Pow authorities can still be added into the blockchain. Resuming the conversion without the saved converter state will result in corrupted block log\n";
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

    my = std::make_unique< detail::block_log_conversion_plugin_impl >( *private_key, _hive_chain_id, options.at( "jobs" ).as< size_t >() );

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
