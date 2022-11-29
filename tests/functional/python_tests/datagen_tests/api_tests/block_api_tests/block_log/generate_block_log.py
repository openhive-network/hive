from pathlib import Path

import test_tools as tt

TOTAL_BLOCKS = 120


def prepare_block_log() -> None:
    init_node = tt.InitNode()
    init_node.run()
    init_node.api.debug_node.debug_generate_blocks(debug_key=tt.Account('initminer').private_key, count=TOTAL_BLOCKS,
                                                   skip=0, miss_blocks=0, edit_if_needed=True)
    init_node.close()
    init_node.block_log.copy_to(Path(__file__).parent.absolute())


if __name__ == '__main__':
    prepare_block_log()
