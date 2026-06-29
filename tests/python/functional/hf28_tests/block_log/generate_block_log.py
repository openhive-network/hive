from __future__ import annotations

from pathlib import Path
from typing import Final

import test_tools as tt

# 20 witnesses that the consuming test (`test_showing_problems_on_hardfork_27`) expects to be active.
WITNESSES: Final[list[str]] = [f"witness{i}-alpha" for i in range(20)]

# Fixed genesis so the generator and the test fixture build/replay the very same alternate chain.
GENESIS_TIME: Final[int] = 1_675_810_800  # 2023-02-07T23:00:00 UTC, just before the historical HF28 date
GENESIS_TIME_STR: Final[str] = "2023-02-07T23:00:00"

# Hardforks are scheduled by block number (deterministic). The block log is generated while the chain is
# still on HF27; the consuming test lets the replayed node keep producing until it reaches HF28.
HF28_ACTIVATION_BLOCK: Final[int] = 90


def prepare_blocklog_on_hf_27() -> None:
    node = tt.InitNode()
    node.config.block_log_split = -1
    for witness in WITNESSES:
        node.config.witness.append(witness)
        node.config.private_key.append(tt.Account(witness).private_key)

    acs = tt.AlternateChainSpecs(
        genesis_time=GENESIS_TIME,
        hardfork_schedule=[
            tt.HardforkSchedule(hardfork=27, block_num=1),
            tt.HardforkSchedule(hardfork=28, block_num=HF28_ACTIVATION_BLOCK),
        ],
        init_supply=400_000_000_000,
        hbd_init_supply=30_000_000_000,
        initial_vesting=tt.InitialVesting(vests_per_hive=1800, hive_amount=50_000_000_000),
    )
    # Pace block production from genesis so the node does not fast-forward past the HF28 block; this keeps
    # the generated block log on HF27.
    node.run(
        alternate_chain_specs=acs,
        time_control=tt.StartTimeControl(start_time=tt.Time.parse(GENESIS_TIME_STR)),
    )

    wallet = tt.Wallet(attach_to=node)
    for witness in WITNESSES:
        wallet.api.import_key(tt.PrivateKey(witness))

    # Create the 20 alpha witness accounts with their own keys, register them as witnesses and vote them
    # into the active schedule (initminer's genesis stake backs every vote). Without this the consuming
    # WitnessNode - which only holds the alpha keys - can never produce a block and never goes live.
    with wallet.in_single_transaction():
        for witness in WITNESSES:
            key = tt.PublicKey(witness)
            wallet.api.create_account_with_keys("initminer", witness, "{}", key, key, key, key)
    with wallet.in_single_transaction():
        for witness in WITNESSES:
            wallet.api.transfer_to_vesting("initminer", witness, tt.Asset.Test(1000))
    with wallet.in_single_transaction():
        for witness in WITNESSES:
            wallet.api.update_witness(
                witness,
                "https://" + witness,
                tt.Account(witness).public_key,
                {"account_creation_fee": tt.Asset.Test(3), "maximum_block_size": 65536, "hbd_interest_rate": 0},
            )
    with wallet.in_single_transaction():
        for witness in WITNESSES:
            wallet.api.vote_for_witness("initminer", witness, True)

    # Wait two schedule rounds so the alpha witnesses become the active producers.
    node.wait_number_of_blocks(42)

    active_witnesses = node.api.database.get_active_witnesses().witnesses
    assert set(WITNESSES).issubset(set(active_witnesses)), f"alpha witnesses are not active: {active_witnesses}"

    last_hardfork = node.api.database.get_hardfork_properties().last_hardfork
    assert last_hardfork == 27, f"Block log must end on HF27, got HF{last_hardfork}"
    assert node.get_last_block_number() < HF28_ACTIVATION_BLOCK, "Block log already reached the HF28 block"

    tt.logger.info(f"Active witnesses: {active_witnesses}")
    tt.logger.info(f"Final block_log head block number: {node.get_last_block_number()}")
    node.close()

    output_block_log_directory = Path(__file__).parent.absolute()
    acs.export_to_file(output_block_log_directory)
    node.block_log.copy_to(output_block_log_directory)


if __name__ == "__main__":
    prepare_blocklog_on_hf_27()
