#include <boost/exception/diagnostic_information.hpp>
#include <boost/program_options.hpp>

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
      ;

    bpo::variables_map options;

    bpo::store( bpo::parse_command_line(argc, argv, opts), options );

    if( options.count("help") )
    {
      std::cout << opts << "\n";
      return 0;
    }

    return 0;
  }
  catch ( const boost::exception& e )
  {
    std::cerr << boost::diagnostic_information(e) << "\n";
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
