from __future__ import annotations

import os
from copy import deepcopy
from pathlib import Path
from typing import Final

import shared_tools.networks_architecture as networks
import test_tools as tt
from hive_local_tools.constants import TRANSACTION_TEMPLATE
from hive_local_tools.functional.python.datagen.recurrent_transfer import execute_function_in_threads
from shared_tools.complex_networks import generate_networks, prepare_network

AMOUNT_OF_ALL_COMMENTS: Final[int] = 60
AMOUNT_OF_ALL_VOTERS: Final[int] = 50_000
ACCOUNTS_PER_CHUNK: Final[int] = 512
MAX_WORKERS: Final[int] = 2048

CONFIG = {
    "networks": [
        {"InitNode": True, "FullApiNode": 4, "WitnessNodes": [1, 1, 1]},
    ]
}


def prepare_blocklog_network():
    """
    Creating a block log with a network based on the configuration.
    In one network:
    - 1 InitNode,
    - 4 FullApiNodes,
    - 3 WitnessNodes (each with one witness).
    """
    architecture = networks.NetworksArchitecture()
    architecture.load(CONFIG)

    tt.logger.info(architecture)
    generate_networks(architecture, Path("base_network_block_log"))


def prepare_blocklog_with_comments_and_votes():
    """
    1) Function continues creation of block_log with network dependencies (from function `prepare_blocklog_network`)
    2) Comments are created (AMOUNT_OF_ALL_COMMENTS)
    3) Accounts are created that vote for comments (AMOUNT_OF_ALL_VOTERS)
    4) Voting takes place (each account casts a vote). The time from the beginning to the end of voting does not exceed
       one hour.
    5) To get all the votes at the required cashout time ( 1h ), you need to slow down node "x0.5"
    """
    architecture = networks.NetworksArchitecture(False)
    architecture.load(CONFIG)

    tt.logger.info(architecture)
    block_log = Path(__file__).with_name("base_network_block_log")
    network = prepare_network(architecture, block_log)

    init_node = network.networks[0].node("InitNode0")
    api_node = network.networks[0].node("FullApiNode0")

    init_wallet = tt.Wallet(attach_to=init_node)
    init_wallet.api.set_transaction_expiration(1000)

    init_wallet.api.import_key(tt.Account("voter").private_key)
    witnesses = init_wallet.api.list_witnesses("", 100, True)

    # change block size to 2 mb
    with init_wallet.in_single_transaction():
        for witness_name in witnesses:
            init_wallet.api.update_witness(
                witness_name,
                "http://url.html",
                tt.Account(witness_name).public_key,
                {"account_creation_fee": tt.Asset.Test(3), "maximum_block_size": 2097152, "hbd_interest_rate": 1000},
            )
    tt.logger.info("Wait 43 blocks...")
    init_node.wait_number_of_blocks(43)  # wait for the block size to change to 2mb

    tt.logger.info("Started funding and creating voters...")
    voters = [f"voter-{i}" for i in range(AMOUNT_OF_ALL_VOTERS)]
    execute_function_in_threads(
        __generate_and_broadcast_transaction,
        args=(init_wallet, __create_and_fund_voter, None),
        args_sequences=(voters,),
        amount=AMOUNT_OF_ALL_VOTERS,
        chunk_size=ACCOUNTS_PER_CHUNK,
        max_workers=MAX_WORKERS,
    )
    tt.logger.info("Finished creating voters.")

    tt.logger.info("Creating comments started...")
    create_comments(init_wallet)
    tt.logger.info("Creating comments finished...")

    tt.logger.info(f"Started voting at: {api_node.get_head_block_time()}")
    for comment_number in range(AMOUNT_OF_ALL_COMMENTS):
        tt.logger.info(f"Started voting for comment: {comment_number}")
        execute_function_in_threads(
            __generate_and_broadcast_transaction,
            args=(init_wallet, __vote_for_comment, comment_number),
            args_sequences=(voters,),
            amount=AMOUNT_OF_ALL_VOTERS,
            chunk_size=ACCOUNTS_PER_CHUNK,
            max_workers=MAX_WORKERS,
        )
    tt.logger.info(f"Finished voting at: {api_node.get_head_block_time()}")
    wait_for_comment_payment(init_node)

    timestamp = init_node.api.block.get_block(block_num=init_node.get_last_block_number())["block"]["timestamp"]
    tt.logger.info(f"Final block_log head block number: {init_node.get_last_block_number()}")
    tt.logger.info(f"Final block_log head block timestamp: {timestamp}")

    init_node.close()
    init_node.block_log.copy_to(Path(__file__).parent.absolute())


def wait_for_comment_payment(node):
    cashout_time = tt.Time.parse(
        node.api.database.find_comments(comments=[["creator-0", "post-creator-0"]])["comments"][0]["cashout_time"]
    )
    tt.logger.info(f"Cashout time: {cashout_time}")

    while True:
        headblock_time = node.get_head_block_time()
        tt.logger.info(f"Head block time: {headblock_time}")
        if headblock_time <= cashout_time - tt.Time.seconds(60):
            node.wait_number_of_blocks(1)
        else:
            tt.logger.info(f"Finished waiting: {headblock_time}")
            break


def create_comments(wallet):
    accounts = wallet.create_accounts(number_of_accounts=AMOUNT_OF_ALL_COMMENTS, name_base="creator")

    with wallet.in_single_transaction():
        for account in accounts:
            wallet.api.transfer_to_vesting("initminer", account.name, tt.Asset.Test(10))
    with wallet.in_single_transaction():
        for account in accounts:
            wallet.api.post_comment(
                account.name,
                f"post-{account.name}",
                "",
                f"test-parent-permlink-{account.name}",
                f"test-title-{account.name}",
                f"test-body-{account.name}",
                "{}",
            )


def __vote_for_comment(voter: str, creator_number: str) -> list:
    return [
        [
            "vote",
            {
                "voter": f"{voter}",
                "author": f"creator-{creator_number}",
                "permlink": f"post-creator-{creator_number}",
                "weight": int(50 * 10000 / AMOUNT_OF_ALL_COMMENTS),
            },
        ]
    ]


def __generate_and_broadcast_transaction(
    wallet: tt.Wallet, func, comment_number: int | None, account_names: list[str]
) -> None:
    transaction = deepcopy(TRANSACTION_TEMPLATE)

    for account_number, name in enumerate(account_names):
        if comment_number is not None:
            if account_number == 0 and comment_number == 0:
                # The account listed first in the transaction bears the transaction cost. For this reason,
                # accounts that incur transaction costs have delegated RCs.
                wallet.api.delegate_rc("initminer", [name], 40_000_000_000)
            transaction["operations"].extend(func(name, creator_number=comment_number))
        else:
            transaction["operations"].extend(func(name))

    wallet.api.sign_transaction(transaction)

    tt.logger.info(f"Finished: {account_names[-1]}")


def __create_and_fund_voter(voter: str) -> list:
    vest_amount = tt.Asset.Test(10)
    key = tt.PublicKey("voter")
    return [
        [
            "account_create",
            {
                "fee": str(tt.Asset.Test(3)),
                "creator": "initminer",
                "new_account_name": voter,
                "owner": {"weight_threshold": 1, "account_auths": [], "key_auths": [[key, 1]]},
                "active": {"weight_threshold": 1, "account_auths": [], "key_auths": [[key, 1]]},
                "posting": {"weight_threshold": 1, "account_auths": [], "key_auths": [[key, 1]]},
                "memo_key": key,
                "json_metadata": "",
            },
        ],
        [
            "transfer_to_vesting",
            {"from": "initminer", "to": voter, "amount": str(vest_amount), "memo": f"supply-vest-transfer-{voter}"},
        ],
    ]


if __name__ == "__main__":
    # Step 1 generate base network with witnesses
    os.environ["GENERATE_NEW_BLOCK_LOG"] = "1"
    prepare_blocklog_network()
    # Step 2 generate accounts, comments and votes
    os.environ.pop("GENERATE_NEW_BLOCK_LOG")
    prepare_blocklog_with_comments_and_votes()
