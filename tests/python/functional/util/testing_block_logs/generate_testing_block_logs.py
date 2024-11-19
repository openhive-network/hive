from __future__ import annotations

import argparse
from pathlib import Path

from loguru import logger

import test_tools as tt


def generate_and_copy_empty_log(
    split_value: int,
    block_count: int,
    target_dir: Path,
) -> None:
    node = tt.InitNode()
    node.config.block_log_split = split_value

    node.run(time_control=tt.SpeedUpRateTimeControl(speed_up_rate=10))
    tt.logger.info(f"Waiting for block {block_count}...")
    node.wait_for_block_with_number(block_count)

    node.close()
    node.block_log.copy_to(target_dir / f"_{block_count}", artifacts="required")


def prepare_empty_logs(
    output_block_log_directory: Path,
) -> None:
    """
    This script generates block logs (both split and monlithic) of different length with empty blocks only
    """
    logger.enable("helpy")
    logger.enable("test_tools")

    # Initial 30 blocks are needed to have at least 10 irreversible ones
    block_log_directory_30 = output_block_log_directory / "empty_30"
    block_log_directory_30.mkdir(parents=True, exist_ok=True)
    generate_and_copy_empty_log(9999, 30, block_log_directory_30)
    generate_and_copy_empty_log(-1, 30, block_log_directory_30)

    # 430 blocks is enough for the split block log to be in 2 parts.
    block_log_directory_430 = output_block_log_directory / "empty_430"
    block_log_directory_430.mkdir(parents=True, exist_ok=True)
    generate_and_copy_empty_log(9999, 430, block_log_directory_430)
    generate_and_copy_empty_log(-1, 430, block_log_directory_430)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-block-log-directory", type=Path, default=Path(__file__).parent)
    args = parser.parse_args()
    prepare_empty_logs(args.output_block_log_directory)
    # TODO: Add generation of other logs here.
