from __future__ import annotations

from pathlib import Path

import test_tools as tt

WITNESSES: list[str] = [f"witness-{witness_num}" for witness_num in range(20)]


def generate_block_log() -> None:
    node = tt.InitNode()
    current_hardfork_number = int(node.get_version()["version"]["blockchain_version"].split(".")[1])

    node.config.witness.extend(WITNESSES)

    acs = tt.AlternateChainSpecs(
        genesis_time=int(tt.Time.now(serialize=False).timestamp()),
        hardfork_schedule=[tt.HardforkSchedule(hardfork=current_hardfork_number, block_num=1)],
        init_witnesses=[f"witness-{witness_num}" for witness_num in range(20)],
    )
    acs.export_to_file(output_dir=Path(__file__).parent)

    node.run(time_control=tt.SpeedUpRateTimeControl(speed_up_rate=5), alternate_chain_specs=acs)

    node.wait_for_irreversible_block(43)

    assert node.api.database.get_dynamic_global_properties().maximum_block_size == 131072

    node.close()
    node.block_log.copy_to(Path(__file__).parent)


if __name__ == "__main__":
    generate_block_log()
