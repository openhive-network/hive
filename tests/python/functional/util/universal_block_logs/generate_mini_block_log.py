from __future__ import annotations

import argparse
from pathlib import Path

import test_tools as tt


def prepare_mini_block_log(output_block_log_directory: Path, name: str) -> None:
    block_log_directory = output_block_log_directory / name

    block_log_config = tt.AlternateChainSpecs(
        genesis_time=int(tt.Time.now(serialize=False).timestamp()),
        hardfork_schedule=[tt.HardforkSchedule(hardfork=28, block_num=0)],
    )

    block_log_config.export_to_file(block_log_directory)

    node = tt.InitNode()
    node.run(time_control=tt.SpeedUpRateTimeControl(speed_up_rate=15))

    node.wait_for_irreversible_block(10)

    node.close()
    node.block_log.copy_to(block_log_directory)
    tt.BlockLog(block_log_directory / "block_log").generate_artifacts()
    tt.logger.info(f"Save block log file to {block_log_directory}")
    tt.logger.info("Contain:")
    for line in block_log_directory.iterdir():
        tt.logger.info(line.name)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-block-log-directory", type=Path, default=Path(__file__).parent)
    args = parser.parse_args()
    prepare_mini_block_log(args.output_block_log_directory, "block_log_single_sign")
    prepare_mini_block_log(args.output_block_log_directory, "block_log_multi_sign")
    prepare_mini_block_log(args.output_block_log_directory, "block_log_open_sign")
