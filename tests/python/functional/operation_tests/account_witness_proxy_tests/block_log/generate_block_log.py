from __future__ import annotations

from datetime import datetime
from pathlib import Path

import test_tools as tt
from hive_local_tools.constants import HIVE_MIN_ROOT_COMMENT_INTERVAL

WITNESSES = ["witness-x", "witness-y", "witness-z"]
PROPOSALS = ["proposal-x", "proposal-y", "proposal-z"]


def prepare_blocklog_with_witnesses_and_active_proposals() -> None:
    node = tt.InitNode()

    for name in WITNESSES:
        node.config.witness.append(name)
        node.config.private_key.append(tt.Account(name).private_key)

    node.run(alternate_chain_specs=set_vest_price_by_alternate_chain_spec())

    wallet = tt.Wallet(attach_to=node)

    for name in WITNESSES:
        wallet.api.import_key(tt.PrivateKey(name))

    # Add witnesses to blockchain
    with wallet.in_single_transaction():
        for name in WITNESSES:
            key = tt.PublicKey(name)
            wallet.api.create_account_with_keys("initminer", name, "{}", key, key, key, key)
    with wallet.in_single_transaction():
        for name in WITNESSES:
            wallet.api.transfer_to_vesting("initminer", name, tt.Asset.Test(10))
    with wallet.in_single_transaction():
        for name in WITNESSES:
            wallet.api.update_witness(
                name,
                "https://" + name,
                tt.Account(name).public_key,
                {"account_creation_fee": tt.Asset.Test(3), "maximum_block_size": 65536, "hbd_interest_rate": 0},
            )

    # Create proposals
    for proposal in PROPOSALS:
        create_proposal(wallet, proposal)
        tt.logger.info(f"{proposal} created, start waiting...")
        node.wait_number_of_blocks(HIVE_MIN_ROOT_COMMENT_INTERVAL * (60 / 3))

    tt.logger.info("Wait 21 blocks to schedule newly created witnesses into future slate")
    node.wait_number_of_blocks(21)

    future_witnesses = node.api.database.get_active_witnesses(include_future=True).future_witnesses
    tt.logger.info(f"Future witnesses after voting: {future_witnesses}")

    tt.logger.info("Wait 21 blocks for future slate to become active slate")
    node.wait_number_of_blocks(21)

    active_witnesses = node.api.database.get_active_witnesses().witnesses
    tt.logger.info(f"Witness state after voting: {active_witnesses}")

    # Reason of this wait is to enable moving forward of irreversible block
    tt.logger.info("Wait 21 blocks (when every witness sign at least one block)")
    node.wait_number_of_blocks(21)

    timestamp = node.api.block.get_block(block_num=node.get_last_block_number()).block.timestamp
    tt.logger.info(f"Final block_log head block number: {node.get_last_block_number()}")
    tt.logger.info(f"Final block_log head block timestamp: {timestamp}")

    node.close()
    node.block_log.copy_to(Path(__file__).parent.absolute())


def create_proposal(wallet: tt.Wallet, permlink: str) -> None:
    wallet.api.post_comment(
        "initminer", f"{permlink}", "", f"parent-{permlink}", f"{permlink}-title", f"{permlink}-body", "{}"
    )
    # create proposal with permlink pointing to comment
    wallet.api.create_proposal(
        "initminer",
        "initminer",
        tt.Time.now(),
        tt.Time.from_now(days=15),
        tt.Asset.Tbd(5),
        permlink,
        f"{permlink}",
    )


def set_vest_price_by_alternate_chain_spec() -> tt.AlternateChainSpecs:
    current_time = datetime.now()
    return tt.AlternateChainSpecs(
        genesis_time=int(current_time.timestamp()),
        hardfork_schedule=[tt.HardforkSchedule(hardfork=28, block_num=1)],
        init_supply=20_000_000_000,
        hbd_init_supply=100_000,
        initial_vesting=tt.InitialVesting(vests_per_hive=1800, hive_amount=10_000_000_000),
    )


if __name__ == "__main__":
    prepare_blocklog_with_witnesses_and_active_proposals()
