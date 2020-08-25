import pytest
import logging

import hive_utils

logger = logging.getLogger()

try:
    from beem import Hive
except Exception as ex:
    logger.exception("beem library is not installed.")
    raise ex

def create_accounts(node, creator, account):
    logger.info("Creating account: {}".format(account['name']))
    node.create_account(account['name'], 
        owner_key=account['public_key'], 
        active_key=account['public_key'], 
        posting_key=account['public_key'],
        memo_key=account['public_key'],
        store_keys = False,
        creator=creator,
        asset='TESTS'
    )
    hive_utils.common.wait_n_blocks(node.rpc.url, 5)


def transfer_to_vesting(node, from_account, account, amount, asset):
    from beem.account import Account
    logger.info("Transfer to vesting from {} to {} amount {} {}".format(
        from_account, account['name'], amount, asset)
    )
    acc = Account(from_account, hive_instance=node)
    acc.transfer_to_vesting(amount, to = account['name'], asset = asset)
    hive_utils.common.wait_n_blocks(node.rpc.url, 5)


def transfer_assets_to_accounts(node, from_account, account, amount, asset):
    from beem.account import Account
    logger.info("Transfer from {} to {} amount {} {}".format(from_account, 
        account['name'], amount, asset)
    )
    acc = Account(from_account, hive_instance=node)
    acc.transfer(account['name'], amount, asset, memo = "initial transfer")
    hive_utils.common.wait_n_blocks(node.rpc.url, 5)


def create_posts(node, account):
    logger.info("Creating posts...")
    from test_utils import get_permlink
    logger.info("New post ==> ({},{},{},{},{})".format(
        "Hivepy proposal title [{}]".format(account['name']), 
        "Hivepy proposal body [{}]".format(account['name']), 
        account['name'], 
        get_permlink(account['name']), 
        "proposals"
    ))
    node.post("Hivepy proposal title [{}]".format(account['name']), 
        "Hivepy proposal body [{}]".format(account['name']), 
        account['name'], 
        permlink = get_permlink(account['name']), 
        tags = "proposals")
    hive_utils.common.wait_n_blocks(node.rpc.url, 5)


def create_proposals(node, account, start_date, end_date, proposal_count):
    logger.info("Creating proposals...")
    from beembase.operations import Create_proposal
    from test_utils import get_permlink
    for idx in range(0, proposal_count):
        logger.info("New proposal ==> ({},{},{},{},{},{},{})".format(
            account['name'], 
            account['name'], 
            start_date, 
            end_date,
            "24.000 TBD",
            "Proposal from account {} {}/{}".format(account['name'], idx, proposal_count),
            get_permlink(account['name'])
        ))
        op = Create_proposal(
            **{
                'creator' : account['name'], 
                'receiver' : account['name'], 
                'start_date' : start_date, 
                'end_date' : end_date,
                'daily_pay' : "24.000 TBD",
                'subject' : "Proposal from account {}".format(account['name']),
                'permlink' : get_permlink(account['name'])
            }
        )
        try:
            node.finalizeOp(op, account['name'], "active")
        except Exception as ex:
            logger.error("Exception: {}".format(ex))
            raise ex
        hive_utils.common.wait_n_blocks(node.rpc.url, 1)
    hive_utils.common.wait_n_blocks(node.rpc.url, 5)


def list_proposals(node, account):
    proposals = node.rpc.list_proposals([account['name']], 1000, "by_creator", "ascending", 'all')
    ids = []
    for proposal in proposals:
        ids.append(int(proposal.get('id', -1)))
    logger.info("Listing proposals: {}".format(ids))
    return ids


def vote_proposals(node, account, ids):
    logger.info("Voting proposals...")
    from beembase.operations import Update_proposal_votes
    op = Update_proposal_votes(
        **{
            'voter' : account["name"],
            'proposal_ids' : ids,
            'approve' : True
        }
    )
    try:
        node.finalizeOp(op, account["name"], "active")
    except Exception as ex:
        logger.error("Exception: {}".format(ex))
        raise ex
    hive_utils.common.wait_n_blocks(node.rpc.url, 5)


def list_voter_proposals(node, account, limit = 1000, last_id = None):
    logger.info("List voted proposals...")
    # last_id is not supported!!!!
    #voter_proposals = node.rpc.list_proposal_votes([account['name']], limit, 'by_voter_proposal', 'ascending', 'all', last_id)
    voter_proposals = node.rpc.list_proposal_votes([account['name']], limit, 'by_voter_proposal', 'ascending', 'all')
    ids = []
    for proposal in voter_proposals:
        if proposal["voter"] == account['name']:
            ids.append(int(proposal['proposal']['proposal_id']))
    return ids


def test_list_voter_proposal(hive_node_provider, pytestconfig):
    logger.info("Testing: remove_proposal")
    assert hive_node_provider is not None, "Node is None"
    assert hive_node_provider.is_running(), "Node is not running"
    node_url = pytestconfig.getoption('--node-url', None)
    assert node_url is not None, '--node-url option not set'
    wif = pytestconfig.getoption('--wif', None)
    assert wif is not None, '--wif option not set'
    creator =  pytestconfig.getoption('--creator', None)
    assert creator is not None, '--creator option not set'

    account = {"name" : "tester001", "private_key" : "5KQeu7SdzxT1DiUzv7jaqwkwv1V8Fi7N8NBZtHugWYXqVFH1AFa", "public_key" : "TST8VfiahQsfS1TLcnBfp4NNfdw67uWweYbbUXymbNiDXVDrzUs7J"}
    
    assert len(account["private_key"]) != 0, "Private key is empty"

    keys = [wif]
    keys.append(account["private_key"])

    node_client = Hive(node = [node_url], no_broadcast = False, 
        keys = keys
    )

    # create accounts
    create_accounts(node_client, creator, account)
    # tranfer to vesting
    transfer_to_vesting(node_client, creator, account, "300.000", 
        "TESTS"
    )
    # transfer assets to accounts
    transfer_assets_to_accounts(node_client, creator, account, 
        "400.000", "TESTS"
    )

    transfer_assets_to_accounts(node_client, creator, account, 
        "400.000", "TBD"
    )

    # create post for valid permlinks
    create_posts(node_client, account)

    import datetime
    import dateutil.parser
    now = node_client.get_dynamic_global_properties().get('time', None)
    if now is None:
        raise ValueError("Head time is None")
    now = dateutil.parser.parse(now)

    start_date = now + datetime.timedelta(days = 1)
    end_date = start_date + datetime.timedelta(days = 2)

    end_date_blocks = start_date + datetime.timedelta(days = 2, hours = 1)

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
    #logger.info("Rest")
    #proposals_ids = list_voter_proposals(node_client, account, 100, proposals_ids[-1])
    #logger.info(proposals_ids)

    assert len(proposals_ids) == 3, "Expecting 3 proposals"
    assert proposals_ids[0] == 0, "Expecting id = 0"
    assert proposals_ids[-1] == 2, "Expecting id = 2"