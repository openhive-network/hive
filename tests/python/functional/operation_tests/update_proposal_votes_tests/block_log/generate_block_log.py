from __future__ import annotations

from pathlib import Path

import test_tools as tt
from hive_local_tools.constants import HIVE_MIN_ROOT_COMMENT_INTERVAL


def prepare_blocklog_with_proposals():
    node = tt.InitNode()

    node.run()
    node.set_vest_price(tt.Asset.Vest(1800))

    wallet = tt.Wallet(attach_to=node)
    create_proposal(wallet, "proposal-x")

    node.wait_number_of_blocks(HIVE_MIN_ROOT_COMMENT_INTERVAL * (60 / 3))

    create_proposal(wallet, "proposal-y")

    node.wait_for_irreversible_block()

    timestamp = node.api.block.get_block(block_num=node.get_last_block_number())["block"]["timestamp"]
    tt.logger.info(f"Final block_log head block number: {node.get_last_block_number()}")
    tt.logger.info(f"Final block_log head block timestamp: {timestamp}")

    with open("timestamp", "w", encoding="utf-8") as file:
        file.write(f"{timestamp}")

    node.close()
    node.block_log.copy_to(Path(__file__).parent.absolute())


def create_proposal(wallet, permlink):
    wallet.api.post_comment(
        "initminer", f"comment-{permlink}", "", f"test-parent-{permlink}", f"{permlink}-title", f"{permlink}-body", "{}"
    )
    # create proposal with permlink pointing to comment
    wallet.api.create_proposal(
        "initminer",
        "initminer",
        tt.Time.now(),
        tt.Time.from_now(days=5),
        tt.Asset.Tbd(5),
        permlink,
        f"comment-{permlink}",
    )


if __name__ == "__main__":
    prepare_blocklog_with_proposals()
