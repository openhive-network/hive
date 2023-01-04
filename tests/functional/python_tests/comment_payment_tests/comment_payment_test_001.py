#!/usr/bin/python3

import sys

sys.path.append("../../")
import hive_utils

from uuid import uuid4
from time import sleep
import logging
import test_utils
import os


LOG_LEVEL = logging.INFO
LOG_FORMAT = "%(asctime)-15s - %(name)s - %(levelname)s - %(message)s"
MAIN_LOG_PATH = "comment_payment_001.log"
log_dir = os.environ.get("TEST_LOG_DIR", None)
if log_dir is not None:
    MAIN_LOG_PATH = log_dir + "/" + MAIN_LOG_PATH
else:
    MAIN_LOG_PATH = "./" + MAIN_LOG_PATH

MODULE_NAME = "Comment-Payment-Tester"
logger = logging.getLogger(MODULE_NAME)
logger.setLevel(LOG_LEVEL)

ch = logging.StreamHandler(sys.stdout)
ch.setLevel(LOG_LEVEL)
ch.setFormatter(logging.Formatter(LOG_FORMAT))

fh = logging.FileHandler(MAIN_LOG_PATH)
fh.setLevel(LOG_LEVEL)
fh.setFormatter(logging.Formatter(LOG_FORMAT))

if not logger.hasHandlers():
    logger.addHandler(ch)
    logger.addHandler(fh)

try:
    from beem import Hive
except Exception as ex:
    logger.error("beem library is not installed.")
    sys.exit(1)


def print_comment_rewards(node_client, accounts):
    logger.info("Print author rewards...")
    from beem.comment import Comment
    from prettytable import PrettyTable

    last_cashout_time = ""

    table = PrettyTable()
    table.field_names = [
        "author",
        "permlink",
        "last_payout",
        "cashout_time",
        "total_vote_weight",
        "reward_weight",
        "total_payout_value",
        "curator_payout_value",
        "author_rewards",
        "net_votes",
        "percent_hbd",
        "pending_payout_value",
        "total_pending_payout_value",
    ]
    ret = []
    for account in accounts:
        blog = node_client.rpc.get_content(account["name"], test_utils.get_post_permlink(account["name"]))
        row = []
        for key in table.field_names:
            row.append(blog[key])
        table.add_row(row)
        ret.append(row)
        last_cashout_time = blog["cashout_time"]
    print(table)
    return last_cashout_time, ret


def compare_tables(before, after):
    logger.info("Comparing tables...")
    for row in range(len(before)):
        for col in range(len(before[row])):
            assert before[row][col] == after[row][col], "{} != {}".format(before[row][col], after[row][col])


if __name__ == "__main__":
    logger.info("Performing comment payment tests")
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("creator", help="Account to create test accounts with")
    parser.add_argument("wif", help="Private key for creator account")
    parser.add_argument("hived_path", help="Path to hived executable.")
    parser.add_argument("replay_hived_path", help="Path to hived executable performing replay.")

    parser.add_argument("--node-url", dest="node_url", default="http://127.0.0.1:8090", help="Url of working hive node")
    parser.add_argument(
        "--working-dir", dest="hived_working_dir", default="/tmp/hived-data/", help="Path to hived working directory"
    )
    parser.add_argument(
        "--config-path",
        dest="hived_config_path",
        default="../../hive_utils/resources/config.ini.in",
        help="Path to source config.ini file",
    )

    args = parser.parse_args()

    node = None

    if args.hived_path:
        logger.info(
            "Running hived via {} in {} with config {}".format(
                args.hived_path, args.hived_working_dir, args.hived_config_path
            )
        )

        node = hive_utils.hive_node.HiveNodeInScreen(args.hived_path, args.hived_working_dir, args.hived_config_path)

    node_url = args.node_url
    wif = args.wif

    if len(wif) == 0:
        logger.error("Private-key is not set in config.ini")
        sys.exit(1)

    logger.info("Using node at: {}".format(node_url))
    logger.info("Using private-key: {}".format(wif))

    accounts = [
        # place accounts here in the format: {'name' : name, 'private_key' : private-key, 'public_key' : public-key}
        {
            "name": "tester001",
            "private_key": "5KQeu7SdzxT1DiUzv7jaqwkwv1V8Fi7N8NBZtHugWYXqVFH1AFa",
            "public_key": "TST8VfiahQsfS1TLcnBfp4NNfdw67uWweYbbUXymbNiDXVDrzUs7J",
        },
        {
            "name": "tester002",
            "private_key": "5KgfcV9bgEen3v9mxkoGw6Rhuf2giDRZTHZjzwisjkrpF4FUh3N",
            "public_key": "TST5gQPYm5bs9dRPHpqBy6dU32M8FcoKYFdF4YWEChUarc9FdYHzn",
        },
        {
            "name": "tester003",
            "private_key": "5Jz3fcrrgKMbL8ncpzTdQmdRVHdxMhi8qScoxSR3TnAFUcdyD5N",
            "public_key": "TST57wy5bXyJ4Z337Bo6RbinR6NyTRJxzond5dmGsP4gZ51yN6Zom",
        },
        {
            "name": "tester004",
            "private_key": "5KcmobLVMSAVzETrZxfEGG73Zvi5SKTgJuZXtNgU3az2VK3Krye",
            "public_key": "TST8dPte853xAuLMDV7PTVmiNMRwP6itMyvSmaht7J5tVczkDLa5K",
        },
        {
            "name": "tester005",
            "private_key": "5Hy4vEeYmBDvmXipe5JAFPhNwCnx7NfsfyiktBTBURn9Qt1ihcA",
            "public_key": "TST7CP7FFjvG55AUeH8riYbfD8NxTTtFH32ekQV4YFXmV6gU8uAg3",
        },
    ]

    before = None
    after = None

    balances_before = None
    balances_after = None

    if not accounts:
        logger.error(
            'Accounts array is empty, please add accounts in a form {"name" : name, "private_key" : private_key, "public_key" : public_key}'
        )
        sys.exit(1)

    keys = [wif]
    for account in accounts:
        keys.append(account["private_key"])

    if node is not None:
        node.run_hive_node(["--enable-stale-production"])
    try:
        if node is not None and node.is_running():
            logger.info("Connecting to testnet...")
            node_client = Hive(node=[node_url], no_broadcast=False, keys=keys)

            logger.info("Publishing feed...")
            from beem.witness import Witness

            w = Witness("initminer", hive_instance=node_client)
            w.feed_publish(1000.000, account="initminer")
            hive_utils.common.wait_n_blocks(node_client.rpc.url, 5)

            logger.info("Chain prefix is: {}".format(node_client.prefix))
            logger.info("Chain ID is: {}".format(node_client.get_config()["HIVE_CHAIN_ID"]))

            # create accounts
            test_utils.create_accounts(node_client, args.creator, accounts)
            # tranfer to vesting
            test_utils.transfer_to_vesting(node_client, args.creator, accounts, "3000000.000", "TESTS")
            # transfer assets to accounts
            test_utils.transfer_assets_to_accounts(node_client, args.creator, accounts, "10000.000", "TESTS")
            test_utils.transfer_assets_to_accounts(node_client, args.creator, accounts, "10000.000", "TBD")

            logger.info("Balances for accounts after initial transfer")
            test_utils.print_balance(node_client, accounts)

            # create post for valid permlinks
            from beem.account import Account

            test_utils.create_posts(node_client, accounts)
            logger.info("Voting...")
            for account in accounts[1:]:
                acc = Account(account["name"], hive_instance=node_client)
                node_client.vote(
                    100.0, "@{}/{}".format("tester001", test_utils.get_post_permlink("tester001")), account=acc
                )
            hive_utils.common.wait_n_blocks(node_client.rpc.url, 10)

            last_cashout_time, _ = print_comment_rewards(node_client, accounts)
            logger.info(
                "Last block number: {}".format(node_client.get_dynamic_global_properties(False)["head_block_number"])
            )
            logger.info("Accelerating time...")
            hive_utils.debug_generate_blocks_until(node_client.rpc.url, wif, last_cashout_time, False)
            hive_utils.debug_generate_blocks(node_client.rpc.url, wif, 100)
            _, before = print_comment_rewards(node_client, accounts)

            logger.info("Balances for accounts at end of the test")
            balances_before = test_utils.print_balance(node_client, accounts)
            logger.info(
                "Last block number: {}".format(node_client.get_dynamic_global_properties(False)["head_block_number"])
            )
            if node is not None:
                node.stop_hive_node()

        logger.info("Attempting replay!!!")
        if args.replay_hived_path:
            logger.info(
                "Replaying with hived via {} in {} with config {}".format(
                    args.replay_hived_path, args.hived_working_dir, args.hived_working_dir + "/config.ini"
                )
            )

            node = hive_utils.hive_node.HiveNodeInScreen(
                args.replay_hived_path, args.hived_working_dir, args.hived_working_dir + "/config.ini", True
            )
        if node is not None:
            node.run_hive_node(["--enable-stale-production", "--replay-blockchain"])
        if node is not None and node.is_running():
            logger.info("Connecting to testnet...")
            node_client = Hive(node=[node_url], no_broadcast=False, keys=keys)
            _, after = print_comment_rewards(node_client, accounts)
            logger.info("Balances for accounts at end of the replay")
            balances_after = test_utils.print_balance(node_client, accounts)
            logger.info(
                "Last block number: {}".format(node_client.get_dynamic_global_properties(False)["head_block_number"])
            )
            logger.info("Comparing comment rewards")
            compare_tables(before, after)
            logger.info("Comparing balances")
            compare_tables(balances_before, balances_after)

            if node is not None:
                node.stop_hive_node()
    except Exception as ex:
        logger.error("Exception: {}".format(ex))
        if node is not None:
            node.stop_hive_node()
        sys.exit(1)
