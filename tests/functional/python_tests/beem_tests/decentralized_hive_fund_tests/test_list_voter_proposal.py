# this test checks implementation of last_id field in list_voter_proposal arguments
import datetime

from beem import Hive
from beem.account import Account
from beembase.operations import Create_proposal
from beembase.operations import Update_proposal_votes
import dateutil.parser

from .... import hive_utils
from test_utils import get_permlink
import test_tools as tt
from hive_local_tools.functional.python.beem import NodeClientMaker
from hive_local_tools.functional.python.beem.decentralized_hive_fund import CREATOR


def create_accounts(node, creator, account):
    tt.logger.info(f"Creating account: {account['name']}")
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
    tt.logger.info(f"Transfer to vesting from {from_account} to {account['name']} amount {amount} {asset}")
    acc = Account(from_account, hive_instance=node)
    acc.transfer_to_vesting(amount, to=account["name"], asset=asset)
    hive_utils.common.wait_n_blocks(node.rpc.url, 5)


# transfer initminer pychol "399.000 TESTS" "initial transfer" true
# transfer initminer pychol "398.000 TBD" "initial transfer" true
def transfer_assets_to_accounts(node, from_account, account, amount, asset):
    tt.logger.info(f"Transfer from {from_account} to {account['name']} amount {amount} {asset}")
    acc = Account(from_account, hive_instance=node)
    acc.transfer(account["name"], amount, asset, memo="initial transfer")
    hive_utils.common.wait_n_blocks(node.rpc.url, 5)


def create_posts(node, account):
    tt.logger.info("Creating posts...")

    tt.logger.info(
        f"New post ==> (Hivepy proposal title [{account['name']}],Hivepy proposal body [{account['name']}],{account['name']},{get_permlink(account['name'])},proposals)"
    )
    node.post(
        f"Hivepy proposal title [{account['name']}]",
        f"Hivepy proposal body [{account['name']}]",
        account["name"],
        permlink=get_permlink(account["name"]),
        tags="proposals",
    )
    hive_utils.common.wait_n_blocks(node.rpc.url, 5)


def create_proposals(node, account, start_date, end_date, proposal_count):
    tt.logger.info("Creating proposals...")

    for idx in range(0, proposal_count):
        tt.logger.info(
            f"New proposal ==> ({account['name']},{account['name']},{start_date},{end_date},24.000 TBD,Proposal from account {account['name']} {idx}/{proposal_count},{get_permlink(account['name'])})"
        )
        op = Create_proposal(
            **{
                "creator": account["name"],
                "receiver": account["name"],
                "start_date": start_date,
                "end_date": end_date,
                "daily_pay": "24.000 TBD",
                "subject": f"Proposal from account {account['name']}",
                "permlink": get_permlink(account["name"]),
            }
        )
        try:
            node.finalizeOp(op, account["name"], "active")
        except Exception as ex:
            tt.logger.error(f"Exception: {ex}")
            raise ex
        hive_utils.common.wait_n_blocks(node.rpc.url, 1)
    hive_utils.common.wait_n_blocks(node.rpc.url, 5)


def list_proposals(node, account):
    proposals = node.rpc.list_proposals([account["name"]], 1000, "by_creator", "ascending", "all")
    ids = []
    for proposal in proposals:
        ids.append(int(proposal.get("id", -1)))
    tt.logger.info(f"Listing proposals: {ids}")
    return ids


def vote_proposals(node, account, ids):
    tt.logger.info("Voting proposals...")

    op = Update_proposal_votes(**{"voter": account["name"], "proposal_ids": ids, "approve": True})
    try:
        node.finalizeOp(op, account["name"], "active")
    except Exception as ex:
        tt.logger.error(f"Exception: {ex}")
        raise ex
    hive_utils.common.wait_n_blocks(node.rpc.url, 5)


def list_voter_proposals(node, account, limit=1000, last_id=None):
    tt.logger.info("List voted proposals...")
    # last_id is not supported!!!!
    # voter_proposals = node.rpc.list_proposal_votes([account['name']], limit, 'by_voter_proposal', 'ascending', 'all', last_id)
    voter_proposals = node.rpc.list_proposal_votes([account["name"]], limit, "by_voter_proposal", "ascending", "all")
    ids = []
    for proposal in voter_proposals:
        if proposal["voter"] == account["name"]:
            ids.append(int(proposal["proposal"]["proposal_id"]))
    return ids


def test_list_voter_proposal(node_client: NodeClientMaker):
    # account = {"name" : "tester001", "private_key" : "", "public_key" : ""}
    account = {
        "name": "tester001",
        "private_key": "5KQeu7SdzxT1DiUzv7jaqwkwv1V8Fi7N8NBZtHugWYXqVFH1AFa",
        "public_key": "TST8VfiahQsfS1TLcnBfp4NNfdw67uWweYbbUXymbNiDXVDrzUs7J",
    }

    node_client = node_client(accounts=[account])

    # create accounts
    create_accounts(node_client, CREATOR, account)
    # tranfer to vesting
    transfer_to_vesting(node_client, CREATOR, account, "300.000", "TESTS")
    # transfer assets to accounts
    transfer_assets_to_accounts(node_client, CREATOR, account, "400.000", "TESTS")

    transfer_assets_to_accounts(node_client, CREATOR, account, "400.000", "TBD")

    # create post for valid permlinks
    create_posts(node_client, account)

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

    tt.logger.info("All")
    proposals_ids = list_voter_proposals(node_client, account)
    tt.logger.info(proposals_ids)

    tt.logger.info("First three")
    proposals_ids = list_voter_proposals(node_client, account, 3)
    tt.logger.info(proposals_ids)

    # last_id not supported!!!
    # tt.logger.info("Rest")
    # proposals_ids = list_voter_proposals(node_client, account, 100, proposals_ids[-1])
    # tt.logger.info(proposals_ids)

    assert len(proposals_ids) == 3, "Expecting 3 proposals"
    assert proposals_ids[0] == 0, "Expecting id = 0"
    assert proposals_ids[-1] == 2, "Expecting id = 2"
