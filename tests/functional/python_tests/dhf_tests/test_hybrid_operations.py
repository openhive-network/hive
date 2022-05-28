#!/usr/bin/python3

import sys

sys.path.append("../../")
import hive_utils

import logging
import os

LOG_LEVEL = logging.INFO
LOG_FORMAT = "%(asctime)-15s - %(name)s - %(levelname)s - %(message)s"
MAIN_LOG_PATH = "hybrid_operations_test_001.log"
log_dir = os.environ.get("TEST_LOG_DIR", None)
if log_dir is not None:
    MAIN_LOG_PATH = log_dir + "/" + MAIN_LOG_PATH
else:
    MAIN_LOG_PATH = "./" + MAIN_LOG_PATH

MODULE_NAME = "Hybrid-Ops-Tests"
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


def transfer_assets_to_accounts(node, from_account, accounts, amount, asset, wif=None):
    from beem.account import Account

    for acnt in accounts:
        logger.info("Transfer from {} to {} amount {} {}".format(from_account, acnt["name"], amount, asset))
        acc = Account(from_account, hive_instance=node)
        acc.transfer(acnt["name"], amount, asset, memo="initial transfer")
    if wif is not None:
        hive_utils.debug_generate_blocks(node.rpc.url, wif, 5)
    else:
        hive_utils.common.wait_n_blocks(node.rpc.url, 5)


def transfer_to_vesting(node, from_account, accounts, amount, asset):
    from beem.account import Account

    for acnt in accounts:
        logger.info("Transfer to vesting from {} to {} amount {} {}".format(from_account, acnt["name"], amount, asset))
        acc = Account(from_account, hive_instance=node)
        acc.transfer_to_vesting(amount, to=acnt["name"], asset=asset)
    hive_utils.common.wait_n_blocks(node.rpc.url, 5)


if __name__ == "__main__":
    logger.info("Performing hybrid operations tests")
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("creator", help="Account to create test accounts with")
    parser.add_argument("wif", help="Private key for creator account")
    parser.add_argument("--node-url", dest="node_url", default="http://127.0.0.1:8090", help="Url of working hive node")
    parser.add_argument(
        "--run-hived",
        dest="hived_path",
        help="Path to hived executable. Warning: using this option will erase contents of selected hived working directory.",
    )
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
    ]

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
        if node is None or node.is_running():
            node_client = Hive(node=[node_url], no_broadcast=False, keys=keys)

            logger.info("Chain prefix is: {}".format(node_client.prefix))
            logger.info("Chain ID is: {}".format(node_client.get_config()["HIVE_CHAIN_ID"]))

            # create test account
            logger.info("Creating account: {}".format(accounts[0]["name"]))
            node_client.create_account(
                accounts[0]["name"],
                owner_key=accounts[0]["public_key"],
                active_key=accounts[0]["public_key"],
                posting_key=accounts[0]["public_key"],
                memo_key=accounts[0]["public_key"],
                store_keys=False,
                creator=args.creator,
                asset="TESTS",
            )
            hive_utils.common.wait_n_blocks(node_client.rpc.url, 5)

            transfer_to_vesting(node_client, args.creator, accounts, "300.000", "TESTS")

            transfer_assets_to_accounts(
                node_client,
                args.creator,
                accounts,
                "400.000",
                "TESTS",
            )

            transfer_assets_to_accounts(
                node_client,
                args.creator,
                accounts,
                "400.000",
                "TBD",
            )

            # create comment
            logger.info(
                "New post ==> ({},{},{},{},{})".format(
                    "Hivepy proposal title [{}]".format(accounts[0]["name"]),
                    "Hivepy proposal body [{}]".format(accounts[0]["name"]),
                    accounts[0]["name"],
                    "hivepy-proposal-title-{}".format(accounts[0]["name"]),
                    "proposals",
                )
            )

            node_client.post(
                "Hivepy proposal title [{}]".format(accounts[0]["name"]),
                "Hivepy proposal body [{}]".format(accounts[0]["name"]),
                accounts[0]["name"],
                permlink="hivepy-proposal-title-{}".format(accounts[0]["name"]),
                tags="firstpost",
            )
            hive_utils.common.wait_n_blocks(node_client.rpc.url, 5)

            from beem.transactionbuilder import TransactionBuilder
            from beembase import operations

            exit_code = 0
            # use hybrid op with old keys
            logger.info("Using hybrid op with old keys")
            try:
                exit_code = 1
                tx = TransactionBuilder(hive_instance=node_client)
                ops = []
                op = operations.Comment_options(
                    **{
                        "author": accounts[0]["name"],
                        "permlink": "hivepy-proposal-title-{}".format(accounts[0]["name"]),
                        "max_accepted_payout": "1000.000 TBD",
                        "percent_steem_dollars": 5000,
                        "allow_votes": True,
                        "allow_curation_rewards": True,
                        "prefix": node_client.prefix,
                    }
                )
                ops.append(op)
                tx.appendOps(ops)
                tx.appendWif(accounts[0]["private_key"])
                tx.sign()
                tx.broadcast()
                logger.exception("Expected exception for old style op was not thrown")
            except Exception as ex:
                if str(ex) == "Assert Exception:false: Obsolete form of transaction detected, update your wallet.":
                    logger.info("Expected exception on old style op: {}".format(ex))
                    exit_code = 0
                else:
                    logger.info("Unexpected exception on old style op: {}".format(ex))

            hive_utils.common.wait_n_blocks(node_client.rpc.url, 5)

            # use hybrid op with new keys
            logger.info("Using hybrid op with new keys")
            try:
                tx = TransactionBuilder(hive_instance=node_client)
                ops = []
                op = operations.Comment_options(
                    **{
                        "author": accounts[0]["name"],
                        "permlink": "hivepy-proposal-title-{}".format(accounts[0]["name"]),
                        "max_accepted_payout": "1000.000 TBD",
                        "percent_hbd": 5000,
                        "allow_votes": True,
                        "allow_curation_rewards": True,
                        "prefix": node_client.prefix,
                    }
                )
                ops.append(op)
                tx.appendOps(ops)
                tx.appendWif(accounts[0]["private_key"])
                tx.sign()
                tx.broadcast()
            except Exception as ex:
                logger.exception("Exception on new style op: {}".format(ex))
                exit_code = 1

            hive_utils.common.wait_n_blocks(node_client.rpc.url, 5)

            if node is not None:
                node.stop_hive_node()
            sys.exit(exit_code)
        sys.exit(1)
    except Exception as ex:
        logger.error("Exception: {}".format(ex))
        if node is not None:
            node.stop_hive_node()
        sys.exit(1)
