import sys

sys.path.append("../../")
import logging
from time import sleep
from uuid import uuid4

import hive_utils

LOG_LEVEL = logging.INFO
LOG_FORMAT = "%(asctime)-15s - %(name)s - %(levelname)s - %(message)s"
MAIN_LOG_PATH = "hdf_tester_utils.log"

MODULE_NAME = "Comment-Payment-Tester.Utils"
logger = logging.getLogger(MODULE_NAME)
logger.setLevel(LOG_LEVEL)


def create_accounts(node, creator, accounts):
    for account in accounts:
        logger.info("Creating account: {}".format(account["name"]))
        node.create_account(
            account["name"],
            owner_key=account["public_key"],
            active_key=account["public_key"],
            posting_key=account["public_key"],
            memo_key=account["public_key"],
            store_keys=False,
            creator=creator,
            asset="TESTS",
        )
    hive_utils.common.wait_n_blocks(node.rpc.url, 5)


def transfer_to_vesting(node, from_account, accounts, amount, asset):
    from beem.account import Account

    for acnt in accounts:
        logger.info("Transfer to vesting from {} to {} amount {} {}".format(from_account, acnt["name"], amount, asset))
        acc = Account(from_account, hive_instance=node)
        acc.transfer_to_vesting(amount, to=acnt["name"], asset=asset)
    hive_utils.common.wait_n_blocks(node.rpc.url, 5)


def transfer_assets_to_accounts(node, from_account, accounts, amount, asset):
    from beem.account import Account

    for acnt in accounts:
        logger.info("Transfer from {} to {} amount {} {}".format(from_account, acnt["name"], amount, asset))
        acc = Account(from_account, hive_instance=node)
        acc.transfer(acnt["name"], amount, asset, memo="initial transfer")
    hive_utils.common.wait_n_blocks(node.rpc.url, 5)


def get_post_permlink(account):
    return "post-permlink-{}".format(account)


def create_posts(node, accounts):
    logger.info("Creating posts...")
    for acnt in accounts:
        logger.info(
            "New post ==> ({},{},{},{},{})".format(
                "Post title [{}]".format(acnt["name"]),
                "Post body [{}]".format(acnt["name"]),
                acnt["name"],
                get_post_permlink(acnt["name"]),
                "firstpost",
            )
        )
        node.post(
            "Post title [{}]".format(acnt["name"]),
            "Post body [{}]".format(acnt["name"]),
            author=acnt["name"],
            permlink=get_post_permlink(acnt["name"]),
            reply_identifier=None,
            json_metadata=None,
            comment_options=None,
            community=None,
            app=None,
            tags="firstpost",
            beneficiaries=None,
            self_vote=False,
            parse_body=False,
        )
        hive_utils.common.wait_n_blocks(node.rpc.url, 1)
    hive_utils.common.wait_n_blocks(node.rpc.url, 5)


def print_balance(node, accounts):
    import prettytable
    from beem.account import Account

    table = prettytable.PrettyTable()
    table.field_names = [
        "Account",
        "balance",
        "savings",
        "hbd",
        "savings_hbd",
        "reward_hbd",
        "reward_hive",
        "reward_vesting",
        "reward_vesting_hive",
        "vesting_shares",
        "delegated_vesting_shares",
        "received_vesting_shares",
        "curation_rewards",
        "posting_rewards",
    ]
    balances = []
    for acnt in accounts:
        ret = Account(acnt["name"], hive_instance=node).json()
        data = [
            acnt["name"],
            ret["balance"]["amount"],
            ret["savings_balance"]["amount"],
            ret["hbd_balance"]["amount"],
            ret["savings_hbd_balance"]["amount"],
            ret["reward_hbd_balance"]["amount"],
            ret["reward_hive_balance"]["amount"],
            ret["reward_vesting_balance"]["amount"],
            ret["reward_vesting_hive"]["amount"],
            ret["vesting_shares"]["amount"],
            ret["delegated_vesting_shares"]["amount"],
            ret["received_vesting_shares"]["amount"],
            ret["curation_rewards"],
            ret["posting_rewards"],
        ]
        balances.append(data)
        table.add_row(data)
    print(table)
    return balances
