from __future__ import annotations

from pathlib import Path

import test_tools as tt
from hive_local_tools import create_alternate_chain_spec_file
from hive_local_tools.constants import ALTERNATE_CHAIN_JSON_FILENAME

NUMBER_OF_REPLIES_TO_POST = 200


def prepare_blocklog_with_comments() -> None:
    genesis_time = tt.Time.now(serialize=False)
    with open("genesis_time", "w", encoding="utf-8") as file:
        file.write(f"{int(genesis_time.timestamp())}")

    create_alternate_chain_spec_file(
        genesis_time=int(genesis_time.timestamp()),
        hardfork_schedule=[{"hardfork": 27, "block_num": 1}, {"hardfork": 28, "block_num": 2000}],
    )

    node = tt.InitNode()
    node.run(
        time_offset="+0h x20",
        arguments=["--alternate-chain-spec", str(tt.context.get_current_directory() / ALTERNATE_CHAIN_JSON_FILENAME)],
    )

    wallet = tt.Wallet(attach_to=node)
    wallet.create_account("alice", vests=tt.Asset.Test(10000000))
    wallet.create_account("bob", vests=tt.Asset.Test(100))

    # Alice create root comment
    wallet.api.post_comment("alice", "root-permlink", "", "someone", "test-title", "this is a body", "{}")

    # Bob reply for root comment NUMBER_OF_REPLIES_TO_POST times
    for reply_number in range(NUMBER_OF_REPLIES_TO_POST):
        wallet.api.post_comment(
            "bob",
            f"comment-{reply_number}",
            "alice",
            "root-permlink",
            f"comment-{reply_number}",
            f"body-{reply_number}",
            "",
        )
        tt.logger.info(f"Reply {reply_number} created.")

    node.wait_for_irreversible_block()

    timestamp = node.api.block.get_block(block_num=node.get_last_block_number()).block.timestamp
    tt.logger.info(f"Final block_log head block number: {node.get_last_block_number()}")
    tt.logger.info(f"Final block_log head block timestamp: {timestamp}")

    with open("timestamp", "w", encoding="utf-8") as file:
        file.write(f"{timestamp}")

    node.close()
    node.block_log.copy_to(Path(__file__).parent.absolute())


if __name__ == "__main__":
    prepare_blocklog_with_comments()
