import pytest

import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.api.message_format import create_proposal




@pytest.fixture()
def node():
    node = tt.InitNode()
    node.run()

    return node


@run_for("testnet")
def test_create_order_operation(node):
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account("alice", hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))

    wallet.api.create_order("alice", 1234, tt.Asset.Test(50), tt.Asset.Tbd(50), False, 3600)
    response = node.api.database.get_order_book(limit=10)

    assert len(response["asks"]) != 0


@run_for("testnet")
def test_cancel_order_operation(node):
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account("alice", hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.create_order("alice", 1234, tt.Asset.Test(50), tt.Asset.Tbd(50), False, 3600)

    wallet.api.cancel_order("alice", 1234)
    response = node.api.database.get_order_book(limit=10)

    assert len(response["asks"]) == 0


@run_for("testnet")
def test_vote_for_witness_operation(node):
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account("alice", vests=tt.Asset.Test(100))
    wallet.create_account('bob', vests=tt.Asset.Test(100))

    wallet.api.update_witness("alice", "'http://url.html'", tt.Account("alice").public_key,
                              {'account_creation_fee': tt.Asset.Test(50),
                               'maximum_block_size': 131072,
                               'hbd_interest_rate': 1000})
    wallet.api.vote_for_witness('bob', 'alice', True)
    bob_vote = node.api.database.list_witness_votes(start=["", ""], limit=10, order="by_account_witness")["votes"][0]

    assert bob_vote["witness"] == "alice" and bob_vote["account"] == "bob"


@run_for("testnet")
def test_decline_voting_rights_operation(node):
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account("alice", vests=tt.Asset.Test(100))
    wallet.api.decline_voting_rights("alice", True)

    response = node.api.database.find_decline_voting_rights_requests(accounts=["alice"])

    assert len(response['requests'][0]) != 0


@run_for("testnet")
def test_proxy_operation(node):
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account("alice", vests=tt.Asset.Test(100))
    wallet.create_account("bob", vests=tt.Asset.Test(100))

    wallet.api.set_voting_proxy("alice", "bob")

    assert node.api.condenser.get_accounts(["alice"])[0]["proxy"] == "bob"


@run_for("testnet")
def test_creation_proposal_and_update_proposal_votes(node):
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account('alice', hives=tt.Asset.Test(100), vests=tt.Asset.Test(100), hbds=tt.Asset.Tbd(300))
    wallet.create_account("bob", vests=tt.Asset.Test(100))

    create_proposal(wallet, "alice")
    wallet.api.update_proposal_votes("alice", [0], True)
    wallet.api.update_proposal_votes("bob", [0], True)

    is_proposal_created = isinstance(node.api.condenser.find_proposals([0])[0], dict)
    is_two_proposal_votes_casted = len(node.api.condenser.list_proposal_votes(['alice'], 100, 'by_voter_proposal')) == 2

    assert is_proposal_created and is_two_proposal_votes_casted


@run_for("testnet")
def test_post_comment_and_vote_operation(node):
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account("alice", vests=tt.Asset.Test(100))

    wallet.api.post_comment("alice", "test-permlink", "", "test-parent", "my first comment", "body", "{}")
    wallet.api.vote("initminer", "alice", "test-permlink", 100)

    vote_added = node.api.condenser.get_active_votes("alice", "test-permlink")[0]["voter"] == "initminer"
    comment_added = len(node.api.database.find_comments(comments=[['alice', 'test-permlink']])['comments']) == 1

    assert vote_added and comment_added


