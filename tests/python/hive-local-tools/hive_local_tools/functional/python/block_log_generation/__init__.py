from __future__ import annotations

import argparse
from pathlib import Path


def parse_block_log_generator_args():
    parser = argparse.ArgumentParser(description="Parse arguments for block log generation.")
    parser.add_argument(
        "--output-block-log-directory",
        type=Path,
        default=Path(__file__).parent,
        help="Directory to save the output block log (default: current script directory).",
    )
    parser.add_argument(
        "--input-block-log-directory",
        type=Path,
        help="Optional path to the input block log directory. Required only in certain cases.",
    )
    return parser.parse_args()
