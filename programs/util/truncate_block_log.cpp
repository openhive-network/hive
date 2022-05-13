#include <hive/chain/database.hpp>
#include <hive/protocol/block.hpp>
#include <fc/io/raw.hpp>

int main( int argc, char** argv, char** envp )
{
  try
  {
    if(argc != 4)
      {
      std::cout << "Usage: truncate_block_log <input_block_log_path> <output_block_log_path> <block_number>" << std::endl;
      return 0;
      }

    fc::path blockLogPath(argv[1]);
    const char* maxBlockNumber = argv[3];
    fc::path outputBlockLogPath(argv[2]);

    uint32_t maxBlock = atoi(maxBlockNumber);

    hive::chain::block_log log;

    ilog("Trying to open input block_log file: `${i}'", ("i", blockLogPath));
    ilog("Truncated block_log will be saved into file: `${i}'", ("i", outputBlockLogPath));

    //TODO : These flags should be set by a command line.
    fc::raw::pack_flags _flags;
    log.rewrite(blockLogPath, outputBlockLogPath, maxBlock, _flags);

    log.close();
  }
  catch ( const std::exception& e )
  {
    edump( ( std::string( e.what() ) ) );
  }

  return 0;
}