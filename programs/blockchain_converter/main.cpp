#include <boost/exception/diagnostic_information.hpp>
#include <boost/program_options.hpp>

#include <fc/filesystem.hpp>
#include <fc/exception/exception.hpp>

#include <hive/utilities/key_conversion.hpp>

#include <hive/chain/block_log.hpp>

#include <iostream>

using namespace hive::chain;
using namespace hive::protocol;
using namespace hive::utilities;
namespace bpo = boost::program_options;

/* struct convert_operations_visitor : public fc::visitor< operation >
{
  const account_create_operation& operator()( const account_create_operation& op )const
  {
    op.get().fee = asset( op.get().fee.amount, op.get().fee.symbol ); // TODO: Convert this
    return op;
  }

  // No assets ops
  template< typename T >
  const T& operator()( const T& op )const { return op; }
}; */

int main( int argc, char** argv )
{
  try
  {
    // Setup logging config
    bpo::options_description opts;
      opts.add_options()
      ("help,h", "Print this help message and exit.")
      ("private-key,k", bpo::value< std::string >(), "init miner private key")
      ("chain-id,c", bpo::value< std::string >()->default_value( HIVE_CHAIN_ID ), "new chain ID")
      ("input,i", bpo::value< std::string >(), "input block log")
      ("output,o", bpo::value< std::string >(), "output block log; defaults to [input]_out" )
      ;

    bpo::variables_map options;

    bpo::store( bpo::parse_command_line(argc, argv, opts), options );

    if( options.count("help") || !options.count("private-key") || !options.count("input") || !options.count("chain-id") )
    {
      std::cout << opts << "\n";
      return 0;
    }

    std::string out_file;
    if( !options.count("output") )
      out_file = options["input"].as< std::string >() + "_out";

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

    fc::optional< fc::ecc::private_key > private_key = wif_to_key( options["private-key"].as< std::string >() );
    FC_ASSERT( private_key.valid(), "unable to parse private key" );

    block_log log_in, log_out;

    log_in.open( block_log_in );
    log_out.open( block_log_out );

    for( uint32_t block_num = 1; block_num <= log_in.head()->block_num(); ++block_num )
    {
      fc::optional< signed_block > block = log_in.read_block_by_num( block_num );
      FC_ASSERT( block.valid(), "unable to read block" );

      // TODO: Derived keys

      block->sign( *private_key );

      for( auto transaction = block->transactions.begin(); transaction != block->transactions.end(); ++transaction )
      {
        // transaction->operations = transaction->visit( convert_operations_visitor() );
        for( auto signature = transaction->signatures.begin(); signature != transaction->signatures.end(); ++signature )
          *signature = private_key->sign_compact( transaction->sig_digest( _hive_chain_id ), fc::ecc::fc_canonical );
      }

      block->transaction_merkle_root = block->calculate_merkle_root();

      log_out.append( *block );

      if( block_num % 1000 == 0 )
        std::cout << "Rewritten block: " << block_num << '\r';
    }

    log_in.close();
    log_out.close();

    std::cout << "block_log conversion completed\n";

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
