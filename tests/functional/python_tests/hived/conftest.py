from pathlib import Path
from pytest import fixture
from test_tools import *
from test_tools import constants, context, Wallet

BLOCK_COUNT = 30

@fixture(scope='package')
def block_log() -> Path:
  from time import sleep
  with World(directory=context.get_current_directory()) as world:
    world.set_clean_up_policy(constants.WorldCleanUpPolicy.REMOVE_ONLY_UNNEEDED_FILES)
    logger.info(f'preparing block log with {BLOCK_COUNT} blocks')
    node = world.create_init_node()
    node.run(wait_for_live=True)

    wallet = Wallet(attach_to=node)
    initminer = node.config.witness[0]

    current_block = 0
    while current_block <= BLOCK_COUNT:
      for _ in range(4):
        # put here some dummy trx so block log does not contain just empty blocks
        wallet.api.transfer_to_vesting(from_=initminer, to=initminer, amount=Asset.Test(0.001))

        sleep(0.2)
      current_block = node.get_last_block_number()

    node.close()
    logger.info(f'prepared block log with {BLOCK_COUNT} blocks')
    yield node.get_block_log().get_path()