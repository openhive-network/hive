#include <hive/chain/database.hpp>
#include <hive/protocol/block.hpp>
#include <fc/io/raw.hpp>

int main(int argc, char **argv, char **envp)
{
  hive::chain::block_log log;
  try
  {
    if (argc < 3)
    {
      std::cout << "Usage: ./block_log_to_json <input_block_log_path> <block_number> <offset = 1> <output_json_path = ./block_log_dump.json>" << std::endl;
      return 0;
    }

    fc::path block_log_path(argv[1]);
    const char *block_count_c = argv[2];

    // default parameters
    const char *block_offset_c = (argc >= 4 ? argv[3] : "1");
    fc::path output_dump_path((argc >= 5 ? argv[4] : "block_log_dump.json"));

    uint32_t block_count = atoi(block_count_c);
    uint32_t offset = atoi(block_offset_c);

    ilog("Trying to open input block_log file: `${i}'", ("i", block_log_path));
    ilog("Dumping block_log into json: `${i}'", ("i", output_dump_path));

    std::ofstream os{ output_dump_path.generic_string().c_str() };
    try
    {
      assert(os.good());
      os << "{";
      log.iterate_over_block_log(block_log_path, [&os, &offset, &block_count](const hive::protocol::signed_block& block) -> bool
      {
        if(block.block_num() < offset) return true;

        os << '"' << block.block_num() << "\":" << fc::json::to_string( block ).c_str();
        
        if(block.block_num() == offset + block_count) return false;
        else
        {
          os << ",";
          return true;
        }
      });
    }catch (const std::exception &e)
    {
      edump((std::string(e.what())));
    }
    catch(...)
    {
      edump(("unknown exception"));
    }

    os << "}";
    log.close();
    os.close();
  }
  catch (const std::exception &e)
  {
    edump((std::string(e.what())));
  }
  catch(...)
  {
    edump(("unknown exception"));
  }

  if(log.is_open()) log.close();
  return 0;
}