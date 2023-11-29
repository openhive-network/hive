from __future__ import annotations

from pathlib import Path
from random import randint

import test_tools as tt
from hive_local_tools.constants import ALTERNATE_CHAIN_JSON_FILENAME
from hive_local_tools.functional.python.operation import create_transaction_with_any_operation


def infinite_post_creator():
    node = tt.InitNode()
    node.config.plugin.append("account_history_api")
    node.config.plugin.append("network_broadcast_api")
    node.config.plugin.append("rc_api")
    node.config.plugin.append("transaction_status_api")

    node.run(
        arguments=[
            "--webserver-http-endpoint=0.0.0.0:2500",
            "--alternate-chain-spec",
            str(Path(__file__).parent.joinpath(ALTERNATE_CHAIN_JSON_FILENAME)),
        ]
    )

    config = node.api.database.get_config()
    tt.logger.info(f"HIVE_CHAIN_ID: {config['HIVE_CHAIN_ID']}")
    wallet = tt.Wallet(attach_to=node)

    # Create voter
    wallet.create_account("voter", hives=tt.Asset.Test(1_000_000), vests=tt.Asset.Test(1_000_000))
    voter = node.api.database.find_accounts(accounts=["voter"])["accounts"][0]
    tt.logger.info(f"voter private key: {tt.Account('voter').private_key}")
    tt.logger.info(f"voter posting authority: {voter['posting']}")

    permlink_number = 0
    while True:
        post = wallet.api.post_comment(
            "initminer", f"permlink-{permlink_number}", "", "test-parent-permlink", "title", "body", "{}"
        )
        tt.logger.info(f"Created: {post['operations'][0][1]}")

        generate_comment_options_operation(wallet, permlink_number)
        tt.logger.info(f"Created comment_options operation for comment with permlink-{permlink_number}")

        node.wait_number_of_blocks(randint(3, 10))
        permlink_number += 1


def generate_comment_options_operation(wallet, permlink_num):
    create_transaction_with_any_operation(
        wallet,
        "comment_options",
        author="initminer",
        permlink=f"permlink-{permlink_num}",
        max_accepted_payout="1000000.000 TBD",
        percent_hbd=50,
        allow_votes=True,
        allow_curation_rewards=True,
        extensions=[
            [
                "comment_payout_beneficiaries",
                {
                    "beneficiaries": [
                        {"account": "initminer", "weight": 10000},
                    ]
                },
            ]
        ],
    )


if __name__ == "__main__":
    infinite_post_creator()
