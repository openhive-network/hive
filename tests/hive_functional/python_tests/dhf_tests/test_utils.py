import sys
sys.path.append("../../")
import hive_utils

from uuid import uuid4
from time import sleep
import logging

LOG_LEVEL = logging.INFO

MODULE_NAME = "DHF-Tests.DHF-Utils"
logger = logging.getLogger(MODULE_NAME)
logger.setLevel(LOG_LEVEL)

# create_account "initminer" "pychol" "" true
def create_accounts(node, creator, accounts):
    for account in accounts:
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


# transfer_to_vesting initminer pychol "310.000 TESTS" true
def transfer_to_vesting(node, from_account, accounts, amount, asset):
    from beem.account import Account
    for acnt in accounts:
        logger.info("Transfer to vesting from {} to {} amount {} {}".format(
            from_account, acnt['name'], amount, asset)
        )
        acc = Account(from_account, hive_instance=node)
        acc.transfer_to_vesting(amount, to = acnt['name'], asset = asset)
    hive_utils.common.wait_n_blocks(node.rpc.url, 5)


# transfer initminer pychol "399.000 TESTS" "initial transfer" true
# transfer initminer pychol "398.000 TBD" "initial transfer" true
def transfer_assets_to_accounts(node, from_account, accounts, amount, asset, wif=None):
    from beem.account import Account
    for acnt in accounts:
        logger.info("Transfer from {} to {} amount {} {}".format(from_account, 
            acnt['name'], amount, asset)
        )
        acc = Account(from_account, hive_instance=node)
        acc.transfer(acnt['name'], amount, asset, memo = "initial transfer")
    if wif is not None:
        hive_utils.debug_generate_blocks(node.rpc.url, wif, 5)
    else:
        hive_utils.common.wait_n_blocks(node.rpc.url, 5)


def transfer_assets_to_treasury(node, from_account, treasury_account, amount, asset, wif=None):
    from beem.account import Account
    logger.info("Transfer from {} to {} amount {} {}".format(from_account, 
        treasury_account, amount, asset)
    )
    acc = Account(from_account, hive_instance=node)
    acc.transfer(treasury_account, amount, asset, memo = "initial transfer")
    if wif is not None:
        hive_utils.debug_generate_blocks(node.rpc.url, wif, 5)
    else:
        hive_utils.common.wait_n_blocks(node.rpc.url, 5)


def get_permlink(account):
    return "hivepy-proposal-title-{}".format(account)


def create_posts(node, accounts, wif=None):
    logger.info("Creating posts...")
    for acnt in accounts:
        logger.info("New post ==> ({},{},{},{},{})".format(
            "Hivepy proposal title [{}]".format(acnt['name']), 
            "Hivepy proposal body [{}]".format(acnt['name']), 
            acnt['name'], 
            get_permlink(acnt['name']), 
            "proposals"
        ))
        node.post("Hivepy proposal title [{}]".format(acnt['name']), 
            "Hivepy proposal body [{}]".format(acnt['name']), 
            acnt['name'], 
            permlink = get_permlink(acnt['name']), 
            tags = "proposals")
    if wif is not None:
        hive_utils.debug_generate_blocks(node.rpc.url, wif, 5)
    else:
        hive_utils.common.wait_n_blocks(node.rpc.url, 5)


def create_proposals(node, proposals, wif=None):
    logger.info("Creating proposals...")
    from beembase.operations import Create_proposal
    for proposal in proposals:
        logger.info("New proposal ==> ({},{},{},{},{},{},{})".format(
            proposal['creator'], 
            proposal['receiver'], 
            proposal['start_date'], 
            proposal['end_date'],
            proposal['daily_pay'],
            "Proposal from account {}".format(proposal['creator']),
            get_permlink(proposal['creator'])
        ))

        op = Create_proposal(
            **{
                'creator' : proposal['creator'], 
                'receiver' : proposal['receiver'], 
                'start_date' : proposal['start_date'], 
                'end_date' : proposal['end_date'],
                'daily_pay' : proposal['daily_pay'],
                'subject' : "Proposal from account {}".format(proposal['creator']),
                'permlink' : get_permlink(proposal['creator'])
            }
        )
        node.finalizeOp(op, proposal['creator'], "active")
    if wif is not None:
        hive_utils.debug_generate_blocks(node.rpc.url, wif, 5)
    else:
        hive_utils.common.wait_n_blocks(node.rpc.url, 5)


def vote_proposals(node, accounts, wif=None):
    logger.info("Voting proposals...")
    from beembase.operations import Update_proposal_votes
    for acnt in accounts:
        proposal_set = [x for x in range(0, len(accounts))]
        logger.info("Account {} voted for proposals: {}".format(acnt["name"], ",".join(str(x) for x in proposal_set)))
        op = Update_proposal_votes(
            **{
                'voter' : acnt["name"],
                'proposal_ids' : proposal_set,
                'approve' : True
            }
        )
        node.finalizeOp(op, acnt["name"], "active")
    if wif is not None:
        hive_utils.debug_generate_blocks(node.rpc.url, wif, 5)
    else:
        hive_utils.common.wait_n_blocks(node.rpc.url, 5)


def list_proposals(node, start_date, status):
    proposals = node.rpc.list_proposals([start_date], 1000, "by_start_date", "ascending", status)
    ret = []
    votes = []
    for proposal in proposals:
        ret.append("{}:{}".format(proposal.get('id', 'Error'), proposal.get('total_votes', 'Error')))
        votes.append(int(proposal.get('total_votes', -1)))
    logger.info("Listing proposals with status {} (id:total_votes): {}".format(status, ",".join(ret)))
    return votes


def print_balance(node, accounts):
    from beem.account import Account
    balances = []
    balances_str = []
    for acnt in accounts:
        ret = Account(acnt['name'], hive_instance=node).json()
        hbd = ret.get('hbd_balance', None)
        if hbd is not None:
            hbd = hbd.get('amount')
        balances_str.append("{}:{}".format(acnt['name'], hbd))
        balances.append(hbd)
    logger.info("Balances ==> {}".format(",".join(balances_str)))
    return balances


def date_to_iso(date):
    return date.replace(microsecond=0).isoformat()


def date_from_iso(date_iso_str):
    import dateutil.parser
    return dateutil.parser.parse(date_iso_str)


def get_start_and_end_date(now, start_days_from_now, end_days_from_start):
    from datetime import timedelta
    start_date = now + timedelta(days = start_days_from_now)
    end_date = start_date + timedelta(days = end_days_from_start)

    assert start_date > now, "Start date must be greater than now"
    assert end_date > start_date, "End date must be greater than start date"

    return date_to_iso(start_date), date_to_iso(end_date)