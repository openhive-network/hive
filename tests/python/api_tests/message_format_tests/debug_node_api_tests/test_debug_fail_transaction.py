from __future__ import annotations

import time

from test_tools.exceptions import CommunicationError, FailedToStartExecutableError
from loguru import logger

import test_tools as tt


def test_debug_fail_transaction() -> None:
    # GIVEN
    max_retries = 3
    for attempt in range(max_retries):
        try:
            node = tt.InitNode()
            node.config.plugin.append("queen")

            # Queen plugin will automatically generate a block when the number of transactions is reached
            node.config.queen_tx_count = 1
            node.run(alternate_chain_specs=tt.AlternateChainSpecs(
                genesis_time=int(tt.Time.now(serialize=False).timestamp()),
                hardfork_schedule=[tt.HardforkSchedule(hardfork=28, block_num=1)],
            ))

            wallet = tt.Wallet(attach_to=node)

            # Generate first initial block, need to activate hardforks. This step is necessary to avoid the problem with wallet
            # serializing assets and keys before HF26.
            node.api.debug_node.debug_generate_blocks(
                debug_key=tt.Account("initminer").private_key,
                count=1,
                skip=0,
                miss_blocks=0,
                edit_if_needed=True,
            )

            # WHEN
            tx_to_pass = wallet.api.transfer("initminer", "initminer", tt.Asset.Test(1), "memo1", broadcast=False)
            tx_to_fail = wallet.api.transfer("initminer", "initminer", tt.Asset.Test(2), "memo2", broadcast=False)
            node.api.debug_node.debug_fail_transaction(tx_id=tx_to_fail.transaction_id)

            # THEN
            # Non-failing transaction passes
            node.api.network_broadcast.broadcast_transaction(trx=tx_to_pass)
            head_block_number = node.api.wallet_bridge.get_dynamic_global_properties().head_block_number
            assert head_block_number == 2

            # failing transaction was not applied with newly produced bloc
            node.api.network_broadcast.broadcast_transaction(trx=tx_to_fail)
            head_block_number = node.api.wallet_bridge.get_dynamic_global_properties().head_block_number
            assert head_block_number == 2

            # Test passed, exit retry loop
            return
        except (FailedToStartExecutableError, CommunicationError, TimeoutError) as e:
            if attempt < max_retries - 1:
                logger.warning(f"Test failed with {type(e).__name__} (attempt {attempt + 1}/{max_retries}), retrying...")
                time.sleep(1)
            else:
                raise
