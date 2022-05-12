#include <iostream>

int main( int argc, char** argv, char** envp )
{
  std::cout << "The truncate_block_log tool has been removed, as it wasn't compatible\n";
  std::cout << "with block log compression\n";
  std::cout << "\n";
  std::cout << "If you want to generate a truncated copy of an exiting block log,\n";
  std::cout << "the `compress_block_log` utility will do this via the `--block-count`\n";
  std::cout << "argument.  If you don't wish to use compression in your truncated log\n";
  std::cout << "you can also specify `--decompress`.\n";
  std::cout << "\n";
  std::cout << "You can also use the repair_block_log.sh shell script to truncate\n";
  std::cout << "an existing block in-place (without making a copy)\n";
  return 1;
}
