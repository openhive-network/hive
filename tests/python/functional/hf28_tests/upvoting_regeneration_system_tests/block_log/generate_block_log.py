from __future__ import annotations

from pathlib import Path

import test_tools as tt

NUMBER_OF_REPLIES_TO_POST = 200


def prepare_blocklog_with_comments() -> None:
    genesis_time = tt.Time.now(serialize=False)
    with open("genesis_time", "w", encoding="utf-8") as file:
        file.write(f"{int(genesis_time.timestamp())}")

    node = tt.InitNode()
    node.run(
        time_control="+0h x20",
        alternate_chain_specs=tt.AlternateChainSpecs(
            genesis_time=int(genesis_time.timestamp()),
            hardfork_schedule=[
                tt.HardforkSchedule(hardfork=27, block_num=1),
                tt.HardforkSchedule(hardfork=28, block_num=2000),
            ],
        ),
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

    node.close()
    node.block_log.copy_to(Path(__file__).parent.absolute())


if __name__ == "__main__":
    prepare_blocklog_with_comments()
