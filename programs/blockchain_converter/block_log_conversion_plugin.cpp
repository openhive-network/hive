#include "block_log_conversion_plugin.hpp"

#include <appbase/application.hpp>

#include <fc/exception/exception.hpp>
#include <fc/optional.hpp>
#include <fc/filesystem.hpp>
#include <fc/io/json.hpp>
#include <fc/log/logger.hpp>
#include <fc/variant.hpp>

#include <hive/chain/block_log.hpp>
#include <hive/chain/full_block.hpp>

#include <hive/protocol/authority.hpp>
#include <hive/protocol/config.hpp>
#include <hive/protocol/types.hpp>
#include <hive/protocol/block.hpp>
#include <hive/protocol/hardfork_block.hpp>

#include <hive/utilities/key_conversion.hpp>

#include <boost/program_options.hpp>

#include <string>
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

    block_log_conversion_plugin_impl( const hp::private_key_type& _private_key, const hp::chain_id_type& chain_id, size_t signers_size = 1 )
      : conversion_plugin_impl( _private_key, chain_id, signers_size, true ) {}

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

    fc::time_point_sec head_block_time = HIVE_GENESIS_TIME;

    // Automatically set start_block_num if not set
    if( log_out.head() )
    {
      if( !start_block_num ) // If output block log exists than continue
        start_block_num = log_out.head()->get_block_num() + 1;
      else if( start_block_num != log_out.head()->get_block_num() + 1 )
        wlog( "Continue block ${cbn} mismatch with out head block: ${obn} in block log conversion plugin. Make sure you know what you are doing.",
            ("obn",log_out.head()->get_block_num())("cbn",start_block_num-1)
          );

      head_block_time = log_out.head()->get_block_header().timestamp;
    }
    else if( !start_block_num )
    {
      start_block_num = 1;
    }

    hp::block_id_type last_block_id;

    if( start_block_num > 1 && log_out.head() ) // continuing conversion
    {
      FC_ASSERT( start_block_num <= log_in.head()->get_block_num(), "cannot resume conversion from a block that is not in the block_log",
        ("start_block_num", start_block_num)("log_in_head_block_num", log_in.head()->get_block_num()) );

      ilog("Continuing conversion from the block with number ${block_num}", ("block_num", start_block_num));
      ilog("Validating the chain id...");

      last_block_id = log_out.head()->get_block_id(); // Required to resume the conversion

      // Validate the chain id on conversion resume (in the best-case scenario, the complexity of this check is nearly constant - when the last block in the output block log has transactions with signatures)
      bool chain_id_match = false;
      uint32_t it_block_num = log_out.head()->get_block_num();

      while( !chain_id_match && it_block_num >= 1 )
      {
        if( appbase::app().is_interrupt_request() ) break;
        std::shared_ptr<hive::chain::full_block_type> _full_block = log_out.read_block_by_num( it_block_num );
        FC_ASSERT( _full_block, "unable to read block", ("block_num", it_block_num) );

        const hp::signed_block& block = _full_block->get_block();

        if( block.transactions.size() )
          if( block.transactions.begin()->signatures.size() )
          {
            converter.touch(block);

            const auto& trx = *block.transactions.begin();
            ilog("Comparing signatures in trx ${trx_id} in block ${block_num}:", ("trx_id", trx.id())("block_num", block.block_num()));

            const auto& sig = *block.transactions.begin()->signatures.begin();
            ilog("Previous signature: ${sig}", ("sig", sig));

            const auto sig_other = converter.generate_signature(trx);
            ilog("Current signature: ${sig}", ("sig", sig_other));

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

      dlog("Chain id match");
    }

    if( !stop_block_num || stop_block_num > log_in.head()->get_block_num() )
      stop_block_num = log_in.head()->get_block_num();

    for( ; start_block_num <= stop_block_num && !appbase::app().is_interrupt_request(); ++start_block_num )
    {
      std::shared_ptr<hive::chain::full_block_type> _full_block = log_in.read_block_by_num( start_block_num );
      FC_ASSERT( _full_block, "unable to read block", ("block_num", start_block_num) );

      hp::signed_block block = _full_block->get_block(); // Copy required due to the const reference returned by the get_block function

      if ( ( log_per_block > 0 && start_block_num % log_per_block == 0 ) || log_specific == start_block_num )
        dlog("Rewritten block: ${block_num}. Data before conversion: ${block}", ("block_num", start_block_num)("block", block));

      auto fb = converter.convert_signed_block( block, last_block_id, head_block_time, false );
      last_block_id = fb->get_block_id();
      converter.on_tapos_change();

      if( start_block_num % 1000 == 0 ) // Progress
        ilog("[ ${progress}% ]: ${processed}/${stop_point} blocks rewritten",
          ("progress", int( float(start_block_num) / stop_block_num * 100 ))("processed", start_block_num)("stop_point", stop_block_num));

      log_out.append( fb );

      if ( ( log_per_block > 0 && start_block_num % log_per_block == 0 ) || log_specific == start_block_num )
        dlog("After conversion: ${block}", ("block", block));

      head_block_time = block.timestamp;
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

    if( !converter.has_hardfork( HIVE_HARDFORK_0_17__770 ) )
      wlog("Conversion interrupted before HF17. Pow authorities can still be added into the blockchain. Resuming the conversion without the saved converter state will result in corrupted block log");
  }

} // detail

  block_log_conversion_plugin::block_log_conversion_plugin() {}
  block_log_conversion_plugin::~block_log_conversion_plugin() {}

  void block_log_conversion_plugin::set_program_options( bpo::options_description& cli, bpo::options_description& cfg ) {}

  void block_log_conversion_plugin::plugin_initialize( const bpo::variables_map& options )
  {
    FC_ASSERT( options.count("input"), "You have to specify the input source for the " HIVE_BLOCK_LOG_CONVERSION_PLUGIN_NAME " plugin" );

    auto input_v = options["input"].as< std::vector< std::string > >();

    FC_ASSERT( input_v.size() == 1, HIVE_BLOCK_LOG_CONVERSION_PLUGIN_NAME " accepts only one input block log" );

    std::string out_file;
    if( options.count("output") && options["output"].as< std::vector< std::string > >().size() )
      out_file = options["output"].as< std::vector< std::string > >().at(0);
    else
      out_file = input_v.at(0) + "_out";

    fc::path block_log_in( input_v.at(0) );
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

    my->log_per_block = options["log-per-block"].as< uint32_t >();
    my->log_specific = options["log-specific"].as< uint32_t >();

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
