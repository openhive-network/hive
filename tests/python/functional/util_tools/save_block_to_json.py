from __future__ import annotations

import json
import os
from pathlib import Path
from typing import Final

import test_tools as tt

BLOCK_LOG_PATH: Path = Path(__file__).parent / "block_log"

DIRECTORY_TO_SAVE_CORRECT_DATA: Path = Path("/home/dev/Documents/correct_data")
DIRECTORY_TO_SAVE_WRONG_DATA: Path = Path("/home/dev/Documents/wrong_data")

START_BLOCK_TO_SAVE: Final[int] = 1
END_BLOCK_TO_SAVE: Final[int] = 10


def save_block_to_json(path: Path):
    block_log = tt.BlockLog(path)

    if not os.path.exists(DIRECTORY_TO_SAVE_CORRECT_DATA):
        os.makedirs(DIRECTORY_TO_SAVE_CORRECT_DATA)

    for block_number in range(START_BLOCK_TO_SAVE, END_BLOCK_TO_SAVE + 1):
        if block_number % 10 == 0:
            tt.logger.info(f"Saved {block_number}")
        block = block_log.get_block(block_number)
        block_as_json = block.json(by_alias=True)
        with open(f"{DIRECTORY_TO_SAVE_CORRECT_DATA}/block_{block_number}.json", "w") as file:
            json.dump(block_as_json, file)


if __name__ == "__main__":
    save_block_to_json(BLOCK_LOG_PATH)
