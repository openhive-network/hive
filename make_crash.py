#!/usr/bin/python3

import test_tools as tt
from pathlib import Path

BLOCK_LOG = Path.cwd() / 'block_log'

if not BLOCK_LOG.exists():
    init = tt.InitNode()
    init.run()
    init.wait_number_of_blocks(120)
    init.close()
    init.get_block_log().copy_to(BLOCK_LOG)

node = tt.ApiNode()
node.run(replay_from=BLOCK_LOG, wait_for_live=False)

# here is fail
try:
    tt.logger.info(node.api.block.get_block_range(starting_block_num=60, count=30))
except Exception as e:
    tt.logger.error(f'FAILED: {e}')
tt.logger.info('Press ^C to finish')
