# this test checks implementation of last_id field in list_voter_proposal arguments
# !/usr/bin/python3

import sys

sys.path.append("../../")

import logging
import sys

import hive_utils
import os

LOG_LEVEL = logging.INFO
LOG_FORMAT = "%(asctime)-15s - %(name)s - %(levelname)s - %(message)s"
MAIN_LOG_PATH = "hdf_proposal_payment.log"
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


def create_accounts(node, creator, account):
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


# transfer_to_vesting initminer pychol "310.000 TESTS" true
def transfer_to_vesting(node, from_account, account, amount, asset):
    from beem.account import Account

    logger.info("Transfer to vesting from {} to {} amount {} {}".format(from_account, account["name"], amount, asset))
    acc = Account(from_account, hive_instance=node)
    acc.transfer_to_vesting(amount, to=account["name"], asset=asset)
    hive_utils.common.wait_n_blocks(node.rpc.url, 5)


# transfer initminer pychol "399.000 TESTS" "initial transfer" true
# transfer initminer pychol "398.000 TBD" "initial transfer" true
def transfer_assets_to_accounts(node, from_account, account, amount, asset):
    from beem.account import Account

    logger.info("Transfer from {} to {} amount {} {}".format(from_account, account["name"], amount, asset))
    acc = Account(from_account, hive_instance=node)
    acc.transfer(account["name"], amount, asset, memo="initial transfer")
    hive_utils.common.wait_n_blocks(node.rpc.url, 5)


def create_posts(node, account):
    logger.info("Creating posts...")
    from test_utils import get_permlink

    logger.info(
        "New post ==> ({},{},{},{},{})".format(
            "Hivepy proposal title [{}]".format(account["name"]),
            "Hivepy proposal body [{}]".format(account["name"]),
            account["name"],
            get_permlink(account["name"]),
            "proposals",
        )
    )
    node.post(
        "Hivepy proposal title [{}]".format(account["name"]),
        "Hivepy proposal body [{}]".format(account["name"]),
        account["name"],
        permlink=get_permlink(account["name"]),
        tags="proposals",
    )
    hive_utils.common.wait_n_blocks(node.rpc.url, 5)


def create_proposals(node, account, start_date, end_date, proposal_count):
    logger.info("Creating proposals...")
    from beembase.operations import Create_proposal
    from test_utils import get_permlink

    for idx in range(0, proposal_count):
        logger.info(
            "New proposal ==> ({},{},{},{},{},{},{})".format(
                account["name"],
                account["name"],
                start_date,
                end_date,
                "24.000 TBD",
                "Proposal from account {} {}/{}".format(account["name"], idx, proposal_count),
                get_permlink(account["name"]),
            )
        )
        op = Create_proposal(
            **{
                "creator": account["name"],
                "receiver": account["name"],
                "start_date": start_date,
                "end_date": end_date,
                "daily_pay": "24.000 TBD",
                "subject": "Proposal from account {}".format(account["name"]),
                "permlink": get_permlink(account["name"]),
            }
        )
        try:
            node.finalizeOp(op, account["name"], "active")
        except Exception as ex:
            logger.error("Exception: {}".format(ex))
            raise ex
        hive_utils.common.wait_n_blocks(node.rpc.url, 1)
    hive_utils.common.wait_n_blocks(node.rpc.url, 5)


def list_proposals(node, account):
    proposals = node.rpc.list_proposals([account["name"]], 1000, "by_creator", "ascending", "all")
    ids = []
    for proposal in proposals:
        ids.append(int(proposal.get("id", -1)))
    logger.info("Listing proposals: {}".format(ids))
    return ids


def vote_proposals(node, account, ids):
    logger.info("Voting proposals...")
    from beembase.operations import Update_proposal_votes

    op = Update_proposal_votes(**{"voter": account["name"], "proposal_ids": ids, "approve": True})
    try:
        node.finalizeOp(op, account["name"], "active")
    except Exception as ex:
        logger.error("Exception: {}".format(ex))
        raise ex
    hive_utils.common.wait_n_blocks(node.rpc.url, 5)


def list_voter_proposals(node, account, limit=1000, last_id=None):
    logger.info("List voted proposals...")
    # last_id is not supported!!!!
    # voter_proposals = node.rpc.list_proposal_votes([account['name']], limit, 'by_voter_proposal', 'ascending', 'all', last_id)
    voter_proposals = node.rpc.list_proposal_votes([account["name"]], limit, "by_voter_proposal", "ascending", "all")
    ids = []
    for proposal in voter_proposals:
        if proposal["voter"] == account["name"]:
            ids.append(int(proposal["proposal"]["proposal_id"]))
    return ids


if __name__ == "__main__":
    logger.info("Performing SPS tests")
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

    # account = {"name" : "tester001", "private_key" : "", "public_key" : ""}
    account = {
        "name": "tester001",
        "private_key": "5KQeu7SdzxT1DiUzv7jaqwkwv1V8Fi7N8NBZtHugWYXqVFH1AFa",
        "public_key": "TST8VfiahQsfS1TLcnBfp4NNfdw67uWweYbbUXymbNiDXVDrzUs7J",
    }

    assert len(account["private_key"]) != 0, "Private key is empty"

    keys = [wif]
    keys.append(account["private_key"])

    logger.info(keys)

    if node is not None:
        node.run_hive_node(["--enable-stale-production"])
    try:
        if node is None or node.is_running():
            node_client = Hive(node=[node_url], no_broadcast=False, keys=keys)

            # create accounts
            create_accounts(node_client, args.creator, account)
            # tranfer to vesting
            transfer_to_vesting(node_client, args.creator, account, "300.000", "TESTS")
            # transfer assets to accounts
            transfer_assets_to_accounts(node_client, args.creator, account, "400.000", "TESTS")

            transfer_assets_to_accounts(node_client, args.creator, account, "400.000", "TBD")

            # create post for valid permlinks
            create_posts(node_client, account)

            import datetime
            import dateutil.parser

            now = node_client.get_dynamic_global_properties().get("time", None)
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
            create_proposals(node_client, account, start_date_str, end_date_str, 5)

            proposals_ids = list_proposals(node_client, account)
            assert len(proposals_ids) == 5

            # each account is voting on proposal
            vote_proposals(node_client, account, proposals_ids)

            logger.info("All")
            proposals_ids = list_voter_proposals(node_client, account)
            logger.info(proposals_ids)

            logger.info("First three")
            proposals_ids = list_voter_proposals(node_client, account, 3)
            logger.info(proposals_ids)

            # last_id not supported!!!
            # logger.info("Rest")
            # proposals_ids = list_voter_proposals(node_client, account, 100, proposals_ids[-1])
            # logger.info(proposals_ids)

            assert len(proposals_ids) == 3, "Expecting 3 proposals"
            assert proposals_ids[0] == 0, "Expecting id = 0"
            assert proposals_ids[-1] == 2, "Expecting id = 2"

            if node is not None:
                node.stop_hive_node()
            sys.exit(0)
        sys.exit(1)
    except Exception as ex:
        logger.error("Exception: {}".format(ex))
        if node is not None:
            node.stop_hive_node()
        sys.exit(1)
