#include <cstdio>

int main( int argc, char** argv, char** envp )
{
  std::puts("The truncate_block_log tool has been removed, as it wasn't compatible");
  std::puts("with block log compression");
  std::puts("");
  std::puts("If you want to generate a truncated copy of an exiting block log,");
  std::puts("the `compress_block_log` utility will do this via the `--block-count`");
  std::puts("argument.  If you don't wish to use compression in your truncated log");
  std::puts("you can also specify `--decompress`.");
  std::puts("");
  std::puts("You can also use the `block_log_util truncate` command to truncate");
  std::puts("an existing block in-place (without making a copy)");
  return 1;
}
