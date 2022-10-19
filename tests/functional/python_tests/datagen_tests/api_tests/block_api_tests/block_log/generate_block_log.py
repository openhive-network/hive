from pathlib import Path

import test_tools as tt

TOTAL_BLOCKS = 120


def prepare_block_log() -> None:
    init_node = tt.InitNode()
    init_node.run()
    init_node.wait_number_of_blocks(TOTAL_BLOCKS)
    init_node.close()
    init_node.block_log.copy_to(Path(__file__).parent.absolute())


if __name__ == '__main__':
    prepare_block_log()
