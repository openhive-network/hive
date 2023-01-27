#include "iceberg_generate_plugin.hpp"

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

namespace hive {namespace converter { namespace plugins { namespace iceberg_generate {

  namespace bpo = boost::program_options;

  namespace hp = hive::protocol;

  using hive::utilities::wif_to_key;

namespace detail {

  using hive::chain::block_log;

  class generate_iceberg_ops_from_op_visitor
  {
  public:
    typedef hp::operation result_type;

    generate_iceberg_ops_from_op_visitor() {}

    result_type operator()( hp::account_create_operation& op )const
    {
      op.owner.clear();
      op.active.clear();
      op.posting.clear();
      op.json_metadata.clear();

      return op;
    }

    result_type operator()( hp::account_create_with_delegation_operation& op )const
    {
      op.owner.clear();
      op.active.clear();
      op.posting.clear();
      op.json_metadata.clear();
      op.extensions.clear();

      return op;
    }

    result_type operator()( hp::account_update_operation& op )const
    {
      op.owner.reset();
      op.active.reset();
      op.posting.reset();
      op.json_metadata.clear();

      return op;
    }

    result_type operator()( hp::account_update2_operation& op )const
    {
      op.owner.reset();
      op.active.reset();
      op.posting.reset();
      op.memo_key.reset();
      op.json_metadata.clear();
      op.posting_json_metadata.clear();
      op.extensions.clear();

      return op;
    }

    result_type operator()( hp::comment_operation& op )const
    {
      op.title.clear();
      op.body.clear();
      op.json_metadata.clear();

      return op;
    }

    result_type operator()( hp::create_claimed_account_operation& op )const
    {
      op.owner.clear();
      op.active.clear();
      op.posting.clear();
      op.json_metadata.clear();
      op.extensions.clear();

      return op;
    }

    result_type operator()( hp::transfer_operation& op )const
    {
      op.memo.clear();

      return op;
    }

    result_type operator()( hp::escrow_transfer_operation& op )const
    {
      op.json_meta.clear();

      return op;
    }

    result_type operator()( hp::witness_update_operation& op )const
    {
      op.url.clear();

      return op;
    }

    result_type operator()( hp::witness_set_properties_operation& op )const
    {
      op.extensions.clear();

      return op;
    }

    result_type operator()( hp::custom_operation& op )const
    {
      op.required_auths.clear();
      op.data.clear();

      return op;
    }

    result_type operator()( hp::custom_json_operation& op )const
    {
      op.required_auths.clear();
      op.required_posting_auths.clear();
      op.json.clear();

      return op;
    }

    result_type operator()( hp::custom_binary_operation& op )const
    {
      op.required_owner_auths.clear();
      op.required_active_auths.clear();
      op.required_posting_auths.clear();
      op.required_auths.clear();
      op.data.clear();

      return op;
    }

    result_type operator()( hp::request_account_recovery_operation& op )const
    {
      op.new_owner_authority.clear();
      op.extensions.clear();

      return op;
    }

    result_type operator()( hp::recover_account_operation& op )const
    {
      op.new_owner_authority.clear();
      op.recent_owner_authority.clear();
      op.extensions.clear();

      return op;
    }

    result_type operator()( hp::reset_account_operation& op )const
    {
      op.new_owner_authority.clear();

      return op;
    }

    result_type operator()( hp::change_recovery_account_operation& op )const
    {
      op.extensions.clear();

      return op;
    }

    result_type operator()( hp::transfer_to_savings_operation& op )const
    {
      op.memo.clear();

      return op;
    }

    result_type operator()( hp::transfer_from_savings_operation& op )const
    {
      op.memo.clear();

      return op;
    }

    result_type operator()( hp::recurrent_transfer_operation& op )const
    {
      op.memo.clear();
      op.extensions.clear();

      return op;
    }

    result_type operator()( hp::create_proposal_operation& op )const
    {
      op.subject.clear();
      op.extensions.clear();

      return op;
    }

    result_type operator()( hp::update_proposal_operation& op )const
    {
      op.subject.clear();
      op.extensions.clear();

      return op;
    }

    result_type operator()( hp::update_proposal_votes_operation& op )const
    {
      op.extensions.clear();

      return op;
    }

    result_type operator()( hp::remove_proposal_operation& op )const
    {
      op.extensions.clear();

      return op;
    }

#ifdef HIVE_ENABLE_SMT
    result_type operator()( hp::smt_create_operation& op )const
    {
      op.extensions.clear();

      return op;
    }

    result_type operator()( hp::smt_setup_operation& op )const
    {
      op.extensions.clear();

      return op;
    }

    result_type operator()( hp::smt_setup_emissions_operation& op )const
    {
      op.extensions.clear();

      return op;
    }

    result_type operator()( hp::smt_set_setup_parameters_operation& op )const
    {
      op.extensions.clear();

      return op;
    }

    result_type operator()( hp::smt_set_runtime_parameters_operation& op )const
    {
      op.extensions.clear();

      return op;
    }

    result_type operator()( hp::smt_contribute_operation& op )const
    {
      op.extensions.clear();

      return op;
    }
#endif

    // No signatures modification ops
    template< typename T >
    result_type operator()( const T& op )const
    {
      FC_ASSERT( !op.is_virtual(), "block log should not contain virtual operations" );
      return op;
    }
  };

  class iceberg_generate_plugin_impl final : public conversion_plugin_impl {
  public:
    block_log log_in, log_out;

    iceberg_generate_plugin_impl( const hp::private_key_type& _private_key, const hp::chain_id_type& chain_id, size_t signers_size = 1 )
      : conversion_plugin_impl( _private_key, chain_id, signers_size, true ) {}

    virtual void convert( uint32_t start_block_num, uint32_t stop_block_num ) override;
    void open( const fc::path& input, const fc::path& output );
    void close();
  };

  void iceberg_generate_plugin_impl::open( const fc::path& input, const fc::path& output )
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

  void iceberg_generate_plugin_impl::convert( uint32_t start_block_num, uint32_t stop_block_num )
  {
    FC_ASSERT( log_in.is_open(), "Input block log should be opened before the conversion" );
    FC_ASSERT( log_out.is_open(), "Output block log should be opened before the conversion" );
    FC_ASSERT( log_in.head(), "Your input block log is empty" );

    fc::time_point_sec head_block_time = HIVE_GENESIS_TIME;

    FC_ASSERT( !log_out.head() && !start_block_num,
      HIVE_ICEBERG_GENERATE_CONVERSION_PLUGIN_NAME " plugin currently does not currently support conversion continue" );
    start_block_num = 1;

    const auto init_start_block_num = start_block_num;

    hp::block_id_type last_block_id;

    if( !stop_block_num || stop_block_num > log_in.head()->get_block_num() )
      stop_block_num = log_in.head()->get_block_num();

    // Pre-init: Detect required iceberg operations
    for( ; start_block_num <= stop_block_num && !appbase::app().is_interrupt_request(); ++start_block_num )
    {
      std::shared_ptr<hive::chain::full_block_type> _full_block = log_in.read_block_by_num( start_block_num );
      FC_ASSERT( _full_block, "unable to read block", ("block_num", start_block_num) );

      hp::signed_block block = _full_block->get_block(); // Copy required due to the const reference returned by the get_block function

      if ( ( log_per_block > 0 && start_block_num % log_per_block == 0 ) || log_specific == start_block_num )
        dlog("Rewritten block: ${block_num}. Data before conversion: ${block}", ("block_num", start_block_num)("block", block));

      block.extensions.clear();

      for( auto& tx : block.transactions )
      {
        generate_iceberg_ops_from_op_visitor pci{};

        for( auto& op : tx.operations )
          op = op.visit( pci );
      }

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

    // The actual conversion
    start_block_num = init_start_block_num;
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

  void iceberg_generate_plugin_impl::close()
  {
    if( log_in.is_open() )
      log_in.close();

    if( log_out.is_open() )
      log_out.close();

    if( !converter.has_hardfork( HIVE_HARDFORK_0_17__770 ) )
      wlog("Conversion interrupted before HF17. Pow authorities can still be added into the blockchain. Resuming the conversion without the saved converter state will result in corrupted block log");
  }

} // detail

  iceberg_generate_plugin::iceberg_generate_plugin() {}
  iceberg_generate_plugin::~iceberg_generate_plugin() {}

  void iceberg_generate_plugin::set_program_options( bpo::options_description& cli, bpo::options_description& cfg ) {}

  void iceberg_generate_plugin::plugin_initialize( const bpo::variables_map& options )
  {
    FC_ASSERT( options.count("input"), "You have to specify the input source for the " HIVE_ICEBERG_GENERATE_CONVERSION_PLUGIN_NAME " plugin" );

    auto input_v = options["input"].as< std::vector< std::string > >();

    FC_ASSERT( input_v.size() == 1, HIVE_ICEBERG_GENERATE_CONVERSION_PLUGIN_NAME " accepts only one input block log" );

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

    my = std::make_unique< detail::iceberg_generate_plugin_impl >( *private_key, _hive_chain_id, options.at( "jobs" ).as< size_t >() );

    my->log_per_block = options["log-per-block"].as< uint32_t >();
    my->log_specific = options["log-specific"].as< uint32_t >();

    my->set_wifs( options.count("use-same-key"), options["owner-key"].as< std::string >(), options["active-key"].as< std::string >(), options["posting-key"].as< std::string >() );

    my->open( block_log_in, block_log_out );
  }

  void iceberg_generate_plugin::plugin_startup() {
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
  void iceberg_generate_plugin::plugin_shutdown()
  {
    my->close();
    my->print_wifs();
  }

} } } } // hive::converter::plugins::block_log_conversion
