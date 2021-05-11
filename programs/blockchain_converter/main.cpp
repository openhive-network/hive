#include <boost/exception/diagnostic_information.hpp>
#include <boost/program_options.hpp>

#include <fc/filesystem.hpp>
#include <fc/exception/exception.hpp>
#include <fc/io/json.hpp>

#include <hive/utilities/key_conversion.hpp>

#include <hive/protocol/authority.hpp>

#include <hive/chain/block_log.hpp>

#include <iostream>
#include <memory>
#include <csignal>

#include "converter.hpp"

using namespace hive::chain;
using namespace hive::protocol;
using namespace hive::utilities;
using namespace hive::converter;
namespace bpo = boost::program_options;

namespace
{
  volatile sig_atomic_t stop_flag = 0; // signal flag
}
void signal_handler( int signal ) { stop_flag = signal; }

int main( int argc, char** argv )
{
  try
  {
    signal( SIGINT, signal_handler );

    // Setup converter options
    bpo::options_description generic_opts{"Generic options"};
      generic_opts.add_options()
      ("help,h", "Print this help message and exit.");
    bpo::options_description blocklog_opts{"Block log options"};
      blocklog_opts.add_options()
      ("input,i", bpo::value< std::string >(), "input block log")
      ("output,o", bpo::value< std::string >(), "output block log; defaults to [input]_out" );
    bpo::options_description conversion_opts{"Conversion options"};
      conversion_opts.add_options()
      ("chain-id,c", bpo::value< std::string >()->default_value( HIVE_CHAIN_ID ), "new chain ID")
      ("private-key,k", bpo::value< std::string >()
#ifdef IS_TEST_NET
        ->default_value( key_to_wif(HIVE_INIT_PRIVATE_KEY) )
#endif
      , "private key with which all transactions and blocks will be signed ")
      ("owner-key,O", bpo::value< std::string >(), "owner key of the second authority")
      ("active-key,A", bpo::value< std::string >(), "active key of the second authority")
      ("posting-key,P", bpo::value< std::string >(), "posting key of the second authority");
    bpo::options_description logging_opts{"Logging options"};
      logging_opts.add_options()
      ("log-per-block,l", bpo::value< uint32_t >()->default_value( 0 )->implicit_value( 1 ), "Displays blocks in JSON format every n blocks")
      ("log-specific,s", bpo::value< uint32_t >()->default_value( 0 ), "Displays only block with specified number");

    bpo::options_description cmdline_options;
      cmdline_options.add(generic_opts).add(blocklog_opts).add(conversion_opts).add(logging_opts);

    bpo::positional_options_description pos_opts;
    pos_opts.add("input", -1);

    bpo::variables_map options;
    bpo::store( bpo::command_line_parser(argc, argv).options(cmdline_options).positional(pos_opts).run(), options );

    if( options.count("help") || !options.count("input") )
    {
      std::cout << "Converts mainnet symbols to testnet symbols and adds second authority to all the accounts. Re-signs blocks using given private key.\n"
        << cmdline_options << "\n";
      return 0;
    }

    uint32_t log_per_block = options["log-per-block"].as< uint32_t >();
    uint32_t log_specific = options["log-specific"].as< uint32_t >();

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
      _hive_chain_id = chain_id_type( chain_id_str);
    }
    catch( fc::exception& )
    {
      FC_ASSERT( false, "Could not parse chain_id as hex string. Chain ID String: ${s}", ("s", chain_id_str) );
    }

    fc::optional< private_key_type > private_key = wif_to_key( options["private-key"].as< std::string >() );
    FC_ASSERT( private_key.valid(), "unable to parse the private key" );

    block_log log_in, log_out;

    log_in.open( block_log_in );
    log_out.open( block_log_out );

    blockchain_converter converter( *private_key, _hive_chain_id );

    private_key_type owner_key;
    private_key_type active_key;
    private_key_type posting_key;
    fc::optional< private_key_type > _owner_key = wif_to_key( options.count("owner-key") ? options["owner-key"].as< std::string >() : "" );
    fc::optional< private_key_type > _active_key = wif_to_key( options.count("active-key") ? options["active-key"].as< std::string >() : "" );
    fc::optional< private_key_type > _posting_key = wif_to_key( options.count("posting-key") ? options["posting-key"].as< std::string >() : "" );

    if( options.count("owner-key") && _owner_key.valid() )
    {
      owner_key = *_owner_key;
      if( !options.count( "active-key" ) )
      {
        std::cout << "Note: Using owner key as the active key!\n";
        active_key = owner_key;
      }
      if( !options.count( "posting-key" ) )
      {
        std::cout << "Note: Using owner key as the posting key!\n";
        posting_key = owner_key;
      }
    }
    else
      owner_key = private_key_type::generate();

    if( options.count("active-key") && _active_key.valid() )
    {
      active_key = *_active_key;
      if( !options.count( "owner-key" ) )
      {
        std::cout << "Note: Using active key as the owner key!\n";
        owner_key = active_key;
      }
      if( !options.count( "posting-key" ) )
      {
        std::cout << "Note: Using active key as the posting key!\n";
        posting_key = active_key;
      }
    }
    else
      active_key = private_key_type::generate();

    if( options.count("posting-key") && _posting_key.valid() )
    {
      posting_key = *_posting_key;
      if( !options.count( "owner-key" ) )
      {
        std::cout << "Note: Using posting key as the owner key!\n";
        owner_key = posting_key;
      }
      if( !options.count( "active-key" ) )
      {
        std::cout << "Note: Using posting key as the active key!\n";
        active_key = posting_key;
      }
    }
    else
      posting_key = private_key_type::generate();

    converter.set_second_authority_key( owner_key, authority::owner );
    converter.set_second_authority_key( active_key, authority::active );
    converter.set_second_authority_key( posting_key, authority::posting );

    block_id_type last_block_id = log_out.head() ? log_out.read_head().id() : block_id_type();

    for( uint32_t block_num = block_header::num_from_id( last_block_id ) + 1; block_num <= log_in.head()->block_num() && !stop_flag; ++block_num )
    {
      fc::optional< signed_block > block = log_in.read_block_by_num( block_num );
      FC_ASSERT( block.valid(), "unable to read block" );

      if ( ( log_per_block > 0 && block_num % log_per_block == 0 ) || log_specific == block_num )
      {
        fc::json json_block;
        fc::variant v;
        fc::to_variant( *block, v );

        std::cout << "Rewritten block: " << block_num
          << ". Data before conversion: " << json_block.to_string( v ) << '\n';
      }

      last_block_id = converter.convert_signed_block( *block, last_block_id );

      if( block_num % 1000 == 0 ) // Progress
      {
        std::cout << "[ " << int( float(block_num) / log_in.head()->block_num() * 100 ) << "% ]: " << block_num << '/' << log_in.head()->block_num() << " blocks rewritten.\r";
        std::cout.flush();
      }

      log_out.append( *block );

      if ( ( log_per_block > 0 && block_num % log_per_block == 0 ) || log_specific == block_num )
      {
        fc::json json_block;
        fc::variant v;
        fc::to_variant( *block, v );

        std::cout << "After conversion: " << json_block.to_string( v ) << '\n';
      }
    }

    if( stop_flag )
      std::cerr << "\nUser interrupt detected! Saving conversion state...";
    if( log_out.head() && log_out.read_head().timestamp.sec_since_epoch() <= HIVE_HARDFORK_0_17_TIME )
      std::cerr << "\nSecond authority has not been applied on the accounts created with proof of work operation. Try resuming the conversion process";

    log_in.close();
    log_out.close();

    std::cout << "\nSecond authority wif private keys:\n"
      << "Owner:   " << key_to_wif( owner_key ) << '\n'
      << "Active:  " << key_to_wif( active_key ) << '\n'
      << "Posting: " << key_to_wif( posting_key ) << '\n'
      << "block_log conversion completed\n";

    return 0;
  }
  catch ( const boost::exception& e )
  {
    std::cerr << boost::diagnostic_information(e) << "\n";
  }
  catch ( const fc::exception& e )
  {
    std::cerr << e.to_detail_string() << "\n";
  }
  catch ( const std::exception& e )
  {
    std::cerr << e.what() << "\n";
  }
  catch ( ... )
  {
    std::cerr << "unknown exception\n";
  }

  return -1;
}
