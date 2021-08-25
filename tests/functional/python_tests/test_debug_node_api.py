from test_tools import World, logger
from test_tools.private.node import Node

# from pytest import raises

'''
struct debug_generate_blocks_args 
{
  std::string                               debug_key;
  uint32_t                                  count = 0;
  uint32_t                                  skip = hive::chain::database::skip_nothing;
  uint32_t                                  miss_blocks = 0;
  bool                                      edit_if_needed = true;
};

struct debug_generate_blocks_return
{
  uint32_t                                  blocks = 0;
};

'''
  # gdp_after = node.api.condenser.get_dynamic_global_properties()

def log_stats(node : Node):
  logger.info(f"current on-chain time: {node.api.condenser.get_dynamic_global_properties()['result']['time']}")
  logger.info(f"current block: {node.get_last_block_number()}")

def test_forwarding_blocks(world : World):
  print(flush=True)
  node = world.create_init_node()
  node.config.plugin.append('debug_node')
  debug_key = node.config.private_key[0]
  node.run( wait_for_live=True )

  block_distance = 1200
  log_stats(node)
  logger.info(f"forwarding {block_distance} blocks...")
  node.api.debug_node.debug_generate_blocks( debug_key=debug_key, count=block_distance )
  log_stats(node)

  block_distance = 5
  logger.info( f'waiting {block_distance*3} seconds [{block_distance} blocks]...' )
  node.wait_number_of_blocks( block_distance )
  log_stats(node)
