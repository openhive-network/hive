#!/usr/bin/python3

import sys

sys.path.append("../../")
import hive_utils

import logging
import test_utils
import os

LOG_LEVEL = logging.INFO
LOG_FORMAT = "%(asctime)-15s - %(name)s - %(levelname)s - %(message)s"
MAIN_LOG_PATH = "hdf_proposal_payment_001.log"
log_dir = os.environ.get("TEST_LOG_DIR", None)
if log_dir is not None:
    MAIN_LOG_PATH = log_dir + "/" + MAIN_LOG_PATH
else:
    MAIN_LOG_PATH = "./" + MAIN_LOG_PATH

MODULE_NAME = "DHF-Tests"
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


# 1. create few proposals - in this scenatio all proposals have the same start and end date
# 2. vote on them to show differences in asset distribution (depending on collected votes)
# 3. wait for proposal payment phase
# 4. verify (using account history and by checking regular account balance) that given accounts have been correctly paid.


def create_proposals(node, accounts, start_date, end_date, wif=None):
    logger.info("Creating proposals...")
    from beembase.operations import Create_proposal

    for acnt in accounts:
        logger.info(
            "New proposal ==> ({},{},{},{},{},{},{})".format(
                acnt["name"],
                acnt["name"],
                start_date,
                end_date,
                "24.000 TBD",
                "Proposal from account {}".format(acnt["name"]),
                test_utils.get_permlink(acnt["name"]),
            )
        )
        op = Create_proposal(
            **{
                "creator": acnt["name"],
                "receiver": acnt["name"],
                "start_date": start_date,
                "end_date": end_date,
                "daily_pay": "24.000 TBD",
                "subject": "Proposal from account {}".format(acnt["name"]),
                "permlink": test_utils.get_permlink(acnt["name"]),
            }
        )
        node.finalizeOp(op, acnt["name"], "active")
    if wif is not None:
        hive_utils.debug_generate_blocks(node.rpc.url, wif, 5)
    else:
        hive_utils.common.wait_n_blocks(node.rpc.url, 5)


if __name__ == "__main__":
    logger.info("Performing SPS tests")
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("creator", help="Account to create test accounts with")
    parser.add_argument("treasury", help="Treasury account")
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
    parser.add_argument(
        "--no-erase-proposal",
        action="store_false",
        dest="no_erase_proposal",
        help="Do not erase proposal created with this test",
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

            # create accounts
            test_utils.create_accounts(node_client, args.creator, accounts)
            # tranfer to vesting
            test_utils.transfer_to_vesting(node_client, args.creator, accounts, "300.000", "TESTS")

            logger.info("Wait 30 days for full voting power")
            hive_utils.debug_quick_block_skip(node_client, wif, (30 * 24 * 3600 / 3))
            hive_utils.debug_generate_blocks(node_client.rpc.url, wif, 10)

            # transfer assets to accounts
            test_utils.transfer_assets_to_accounts(node_client, args.creator, accounts, "400.000", "TESTS", wif)

            test_utils.transfer_assets_to_accounts(node_client, args.creator, accounts, "400.000", "TBD", wif)

            logger.info("Balances for accounts after initial transfer")
            test_utils.print_balance(node_client, accounts)
            # transfer assets to treasury
            test_utils.transfer_assets_to_treasury(node_client, args.creator, args.treasury, "1000000.000", "TBD", wif)
            test_utils.print_balance(node_client, [{"name": args.treasury}])

            # create post for valid permlinks
            test_utils.create_posts(node_client, accounts, wif)

            import datetime
            import dateutil.parser

            now = node_client.get_dynamic_global_properties(False).get("time", None)
            if now is None:
                raise ValueError("Head time is None")
            now = dateutil.parser.parse(now)

            start_date = now + datetime.timedelta(days=1)
            end_date = start_date + datetime.timedelta(days=2)

            end_date_blocks = start_date + datetime.timedelta(days=2, hours=1)

            start_date_str = start_date.replace(microsecond=0).isoformat()
            end_date_str = end_date.replace(microsecond=0).isoformat()

            end_date_blocks_str = end_date_blocks.replace(microsecond=0).isoformat()

            # create proposals - each account creates one proposal
            create_proposals(node_client, accounts, start_date_str, end_date_str, wif)

            # list proposals with inactive status, it shoud be list of pairs id:total_votes
            test_utils.list_proposals(node_client, start_date_str, "inactive")

            # each account is voting on proposal
            test_utils.vote_proposals(node_client, accounts, wif)

            # list proposals with inactive status, it shoud be list of pairs id:total_votes
            votes = test_utils.list_proposals(node_client, start_date_str, "inactive")
            for vote in votes:
                # should be 0 for all
                assert vote == 0, "All votes should be equal to 0"

            logger.info("Balances for accounts after creating proposals")
            balances = test_utils.print_balance(node_client, accounts)
            for balance in balances:
                # should be 390.000 TBD for all
                assert balance == "390000", "All balances should be equal to 390.000 TBD"
            test_utils.print_balance(node_client, [{"name": args.treasury}])

            # move forward in time to see if proposals are paid
            # moving is made in 1h increments at a time, after each
            # increment balance is printed
            logger.info("Moving to date: {}".format(start_date_str))
            hive_utils.common.debug_generate_blocks_until(node_client.rpc.url, wif, start_date_str, False)
            current_date = start_date
            while current_date < end_date:
                current_date = current_date + datetime.timedelta(hours=1)
                current_date_str = current_date.replace(microsecond=0).isoformat()
                logger.info("Moving to date: {}".format(current_date_str))
                hive_utils.common.debug_generate_blocks_until(node_client.rpc.url, wif, current_date_str, False)

                logger.info("Balances for accounts at time: {}".format(current_date_str))
                test_utils.print_balance(node_client, accounts)
                test_utils.print_balance(node_client, [{"name": args.treasury}])
                votes = test_utils.list_proposals(node_client, start_date_str, "active")
                for vote in votes:
                    # should be > 0 for all
                    assert vote > 0, "All votes counts shoud be greater than 0"

            # move additional hour to ensure that all proposals ended
            logger.info("Moving to date: {}".format(end_date_blocks_str))
            hive_utils.common.debug_generate_blocks_until(node_client.rpc.url, wif, end_date_blocks_str, False)
            logger.info("Balances for accounts at time: {}".format(end_date_blocks_str))
            balances = test_utils.print_balance(node_client, accounts)
            for balance in balances:
                assert balance == "438000", "All balances should be equal 438000"
            test_utils.print_balance(node_client, [{"name": args.treasury}])
            votes = test_utils.list_proposals(node_client, start_date_str, "expired")
            for vote in votes:
                # should be > 0 for all
                assert vote > 0, "All votes counts shoud be greater than 0"

            if node is not None:
                node.stop_hive_node()
            sys.exit(0)
        sys.exit(1)
    except Exception as ex:
        logger.error("Exception: {}".format(ex))
        if node is not None:
            node.stop_hive_node()
        sys.exit(1)
