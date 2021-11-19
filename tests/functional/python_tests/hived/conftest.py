from pathlib import Path
from typing import Tuple

from pytest import fixture
from test_tools import *
from test_tools import BlockLog, Wallet


@fixture(scope='package')
def block_log_helper() -> Tuple[BlockLog, int]:
    BLOCK_COUNT = 30
    with World() as world:
        logger.info(f'preparing block log with {BLOCK_COUNT} blocks')
        node = world.create_init_node()
        node.run(wait_for_live=True)

        node.wait_for_block_with_number(BLOCK_COUNT)
        block_log_length = node.get_last_block_number()

        node.close()
        logger.info(f'prepared block log with {block_log_length} blocks')
        yield node.get_block_log(), block_log_length


@fixture(scope='package')
def block_log(block_log_helper) -> Path:
    return block_log_helper[0].get_path()


@fixture(scope='package')
def block_log_length(block_log_helper) -> int:
    return block_log_helper[1]
