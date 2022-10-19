from pathlib import Path
from typing import Tuple

from pytest import fixture

import test_tools as tt


@fixture(scope='package')
def block_log_helper() -> Tuple[tt.BlockLog, int]:
    BLOCK_COUNT = 30

    tt.logger.info(f'preparing block log with {BLOCK_COUNT} blocks')
    node = tt.InitNode()
    node.run(wait_for_live=True)

    node.wait_for_block_with_number(BLOCK_COUNT)
    block_log_length = node.get_last_block_number()

    node.close()
    tt.logger.info(f'prepared block log with {block_log_length} blocks')
    yield node.block_log, block_log_length


@fixture(scope='package')
def block_log(block_log_helper) -> Path:
    return block_log_helper[0].get_path()


@fixture(scope='package')
def block_log_length(block_log_helper) -> int:
    return block_log_helper[1]
