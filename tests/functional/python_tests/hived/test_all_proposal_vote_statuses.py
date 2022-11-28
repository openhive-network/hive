from copy import deepcopy
from typing import Final, List

import pytest

import test_tools as tt

from ....local_tools import run_for

VOTERS_AMOUNT: Final[int] = 100
CALL_LIST_PROPOSAL_VOTES_TEMPLATE: Final[dict] = {
    "start": [""],
    "limit": 1000,
    "order": "by_voter_proposal",
    "order_direction": "ascending",
    "status": "STATUS_TO_BE_DEFINED",
}

@run_for("testnet")
@pytest.mark.parametrize("api", ["condenser", "database", "wallet_bridge"])
@pytest.mark.parametrize("status", ["active", "all", "expired", "inactive", "votable"])
def test_proposal_status_filter_on_list_proposal_votes(node, api, status):
    wallet = tt.Wallet(attach_to=node)

    # Create active proposal
    create_account_and_proposal(wallet, "alice", tt.Time.now(), tt.Time.from_now(days=1))
    # Create inactive proposal
    create_account_and_proposal(wallet, "bob", tt.Time.from_now(days=1), tt.Time.from_now(days=3))
    # Create expired proposal
    create_account_and_proposal(wallet, "carol", tt.Time.now(), tt.Time.from_now(seconds=30))

    approve_all_created_proposals(node, wallet)

    wait_for_proposal_expiration(node)

    proposal_votes = call_list_proposal_votes(node, api, status)

    if status in ["active", "inactive", "expired"]:
        assert len(proposal_votes) == VOTERS_AMOUNT  # only active or inactive or expired proposals
    if status == "votable":
        assert len(proposal_votes) == VOTERS_AMOUNT * 2  # active + inactive votes
    if status == "all":
        assert len(proposal_votes) == VOTERS_AMOUNT * 3


def wait_for_proposal_expiration(node: tt.InitNode) -> None:
    start = tt.Time.now(serialize=False)
    while True:
        assert tt.Time.now(serialize=False) - start < tt.Time.minutes(1), "Proposal didn't expire within indicated time"
        expiration_proposal = node.api.database.list_proposals(start=[""],
                                                               limit=100,
                                                               order='by_creator',
                                                               order_direction='ascending',
                                                               status='expired'
                                                               )['proposals']
        if len(expiration_proposal) > 0:
            break

def approve_all_created_proposals(node: tt.InitNode, wallet: tt.Wallet) -> None:
    voters = get_account_names(wallet.create_accounts(VOTERS_AMOUNT))
    proposals_amount = len(node.api.condenser.list_proposals([''], 100, 'by_creator', 'ascending', 'all'))

    with wallet.in_single_transaction():
        for voter in voters:
            wallet.api.transfer_to_vesting("initminer", voter, tt.Asset.Test(0.1))
            for proposal_id in range(proposals_amount):
                wallet.api.update_proposal_votes(voter, [proposal_id], True)


def call_list_proposal_votes(node: tt.InitNode, api: str, status: str) -> dict:
    if api == "database":
        template = deepcopy(CALL_LIST_PROPOSAL_VOTES_TEMPLATE)
        template["status"] = status
        result = getattr(node.api, api).list_proposal_votes(**template)
    else:
        args = list(CALL_LIST_PROPOSAL_VOTES_TEMPLATE.values())
        args[args.index("STATUS_TO_BE_DEFINED")] = status
        result = getattr(node.api, api).list_proposal_votes(*args)

    if api != "condenser":
        return result["proposal_votes"]

    return result


def create_account_and_proposal(
        wallet: tt.Wallet, account_name: str, proposal_start_date: str, proposal_end_date: str
) -> None:
    wallet.create_account(account_name, hives=tt.Asset.Test(100), vests=tt.Asset.Test(100), hbds=tt.Asset.Tbd(100))
    wallet.api.post_comment(
        account_name, f"permlink-{account_name}", "", f"parent-permlink-{account_name}", "title", "body", "{}"
    )
    wallet.api.create_proposal(
        account_name,
        account_name,
        proposal_start_date,
        proposal_end_date,
        tt.Asset.Tbd(1 * 100),
        f"subject-{account_name}",
        f"permlink-{account_name}",
    )


def get_account_names(accounts: List[tt.Account]) -> List[str]:
    return [account.name for account in accounts]
