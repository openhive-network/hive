"""
Test group demonstrating basic usage of the `queen` plugin.
"""
from __future__ import annotations

from typing import Final

import pytest

import test_tools as tt

REQUIRED_TX_NUMBER: Final[int] = 1
MINIMUM_BLOCK_SIZE: Final[int] = 1000


@pytest.fixture()
def chain_specification():
    return tt.AlternateChainSpecs(
        genesis_time=int(tt.Time.now(serialize=False).timestamp()),
        hardfork_schedule=[tt.HardforkSchedule(hardfork=28, block_num=1)],
    )


@pytest.fixture()
def queen_node():
    node = tt.InitNode()
    node.config.plugin.append("queen")
    return node


@pytest.mark.testnet()
def test_control_queen_by_tx_count(chain_specification: tt.AlternateChainSpecs, queen_node: tt.InitNode) -> None:
    # Queen plugin will automatically generate a block when the number of transactions is reached
    queen_node.config.queen_tx_count = REQUIRED_TX_NUMBER
    queen_node.run(alternate_chain_specs=chain_specification)

    wallet = tt.Wallet(attach_to=queen_node)

    # Generate first initial block, need to activate hardforks. This step is necessary to avoid the problem with wallet
    # serializing assets and keys before HF26.
    queen_node.api.debug_node.debug_generate_blocks(
        debug_key=tt.Account("initminer").private_key,
        count=1,
        skip=0,
        miss_blocks=0,
        edit_if_needed=True,
    )

    # Create transactions, blocks will be automatic create every one transaction
    for transfer_number in range(REQUIRED_TX_NUMBER * 5):
        wallet.api.transfer("initminer", "initminer", tt.Asset.Test(1), f"memo-{transfer_number}")
        last_block_number = queen_node.get_last_block_number()
        assert len(queen_node.api.block.get_block(block_num=last_block_number).block.transactions) == REQUIRED_TX_NUMBER

    assert queen_node.get_last_block_number() == 5 + 1


@pytest.mark.testnet()
def test_control_queen_by_block_size(chain_specification: tt.AlternateChainSpecs, queen_node: tt.InitNode) -> None:
    # Queen plugin will automatically generate a block when the minimum transaction size in bytes is reached.
    queen_node.config.queen_block_size = MINIMUM_BLOCK_SIZE
    queen_node.run(alternate_chain_specs=chain_specification)

    wallet = tt.Wallet(attach_to=queen_node)

    # Generate first initial block, need to activate hardforks. This step is necessary to avoid the problem with wallet
    # serializing assets and keys before HF26.
    queen_node.api.debug_node.debug_generate_blocks(
        debug_key=tt.Account("initminer").private_key,
        count=1,
        skip=0,
        miss_blocks=0,
        edit_if_needed=True,
    )

    # Create one transaction larger than MINIMUM_BLOCK_SIZE bytes
    with wallet.in_single_transaction():
        for transfer_number in range(100):
            wallet.api.transfer("initminer", "initminer", tt.Asset.Test(1), f"memo-{transfer_number}")

    last_block_number = queen_node.get_last_block_number()
    assert last_block_number == 2
    assert len(queen_node.api.block.get_block(block_num=last_block_number).block.transactions[0].operations) == 100
