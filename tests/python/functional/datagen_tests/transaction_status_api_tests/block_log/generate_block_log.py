from __future__ import annotations

import argparse
import json
from pathlib import Path

import test_tools as tt


def prepare_block_log_with_witnesses(output_block_log_directory: Path) -> None:
    node = tt.InitNode()
    node.run()
    wallet = tt.Wallet(attach_to=node)

    transaction_ids = []
    number_of_blocks = 1500
    node.wait_for_block_with_number(node.get_last_block_number() + 1)
    while node.get_last_block_number() < number_of_blocks:
        actual_block = node.get_last_block_number() + 1
        transaction = wallet.api.create_account("initminer", f"account{actual_block}", "{}")
        transaction_ids.append({"block_num": transaction["block_num"], "transaction_id": transaction["transaction_id"]})
        tt.logger.info(actual_block)
    with open("transactions_ids.json", "w", encoding="utf-8") as json_file:
        json.dump(transaction_ids, json_file)

    node.close()
    node.block_log.copy_to(output_block_log_directory / "witness_setup")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-block-log-directory", type=Path, default=Path(__file__).parent)
    args = parser.parse_args()
    prepare_block_log_with_witnesses(args.output_block_log_directory)
