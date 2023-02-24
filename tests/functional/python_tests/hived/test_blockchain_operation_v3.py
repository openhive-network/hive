from hive_local_tools import run_for
from hive_local_tools.constants import TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS

import pytest
import re
import test_tools as tt


@pytest.fixture()
def node():
    node = tt.InitNode()
    node.run()

    return node


@run_for("testnet")
def test_try_to_vote_after_decline_voting_rights(node):
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account("alice", vests=tt.Asset.Test(100))
    wallet.api.decline_voting_rights("alice", True)
    node.wait_number_of_blocks(TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)

    try:
        wallet.api.vote_for_witness("alice", "initminer", True)
    except tt.exceptions.CommunicationError as error:
        value_to_check = error.response["error"]["message"]

    assert re.search("declined its voting", value_to_check)


@run_for("testnet")
def test_try_to_vote_by_proxy_after_decline_voting_rights(node):
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account("alice", hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))

    wallet.api.decline_voting_rights("alice", True)
    node.wait_number_of_blocks(TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)

    try:
        wallet.api.set_voting_proxy("alice", "initminer")
    except tt.exceptions.CommunicationError as error:
        value_to_check = error.response["error"]["message"]

    assert re.search("cannot proxy votes", value_to_check)


@run_for("testnet")
def test_start_escrow_transfer_operation_and_approve_by_both_sides(node):
    wallet = tt.Wallet(attach_to=node)
    for name in ["alice", "bob"]:
        wallet.create_account(name, vests=tt.Asset.Test(100))

    wallet.api.escrow_transfer("initminer", "alice", "bob", 1, tt.Asset.Tbd(50), tt.Asset.Test(50), tt.Asset.Tbd(1),
                               tt.Time.from_now(weeks=10),
                               tt.Time.from_now(weeks=20),
                               '{}')
    for name in ["alice", "bob"]:
        wallet.api.escrow_approve("initminer", "alice", "bob", name, 1, True)

    response = node.api.database.list_escrows(start=["", 0], limit=10, order="by_from_id")
    if len(response["escrows"]) != 0:
        to_check = [response["escrows"][0][person] for person in ["agent_approved", "to_approved"]]
    else:
        to_check = [False, False]
    assert all(to_check)


@run_for("testnet")
def test_remove_proposal_and_what_will_happen_with_vote(node):
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account("alice", vests=tt.Asset.Test(100), hbds=tt.Asset.Tbd(200))
    wallet.create_account("bob", vests=tt.Asset.Test(100))
    wallet.api.post_comment("alice", "test-permlink", "", "parent-test", "test-title", "test-body", "{}")
    wallet.api.create_proposal("alice", "bob", tt.Time.now(), tt.Time.from_now(weeks=2), tt.Asset.Tbd(5),
                               "test-subject", "test-permlink")

    wallet.api.update_proposal_votes("bob", [0], True)
    wallet.api.remove_proposal("alice", [0])

    proposals = node.api.condenser.list_proposals(['alice'], 100, 'by_creator', 'ascending', 'all', 0)
    votes = node.api.database.list_proposal_votes(start=[''], limit=100, order='by_voter_proposal',
                                                  order_direction='ascending', status='all')

    assert len(proposals) == 0 and len(votes["proposal_votes"]) == 0













