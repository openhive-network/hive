#include <boost/exception/diagnostic_information.hpp>
#include <boost/program_options.hpp>

#include <fc/filesystem.hpp>
#include <fc/exception/exception.hpp>

#include <hive/utilities/key_conversion.hpp>

#include <iostream>

namespace bpo = boost::program_options;

int main( int argc, char** argv )
{
  try
  {
    // Setup logging config
    boost::program_options::options_description opts;
      opts.add_options()
      ("help,h", "Print this help message and exit.")
      ("private-key,k", bpo::value< std::string >(), "init miner private key")
      ("input,i", bpo::value< std::string >(), "input block log")
      ("output,o", bpo::value< std::string >(), "output block log; defaults to [input]_out" )
      ;

    bpo::variables_map options;

    bpo::store( bpo::parse_command_line(argc, argv, opts), options );

    if( options.count("help") || !options.count("private-key") || !options.count("input") )
    {
      std::cout << opts << "\n";
      return 0;
    }

    std::string out_file;
    if( !options.count("output") )
      out_file = options["input"].as< std::string >() + "_out";

    fc::path block_log_in( options["input"].as< std::string >() );
    fc::path block_log_out( out_file );

    fc::optional< fc::ecc::private_key > private_key = hive::utilities::wif_to_key( options["private-key"].as< std::string >() );
    FC_ASSERT( private_key.valid(), "unable to parse private key" );

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
