import test_tools as tt

from hive_local_tools import run_for
from hive_local_tools.api.message_format import create_proposal


# This test is not performed on 5 million blocklog because proposals feature was introduced after block with number
# 5000000. See the readme.md file in this directory for further explanation.
@run_for('testnet', 'live_mainnet')
def test_list_proposals(node, should_prepare):
    preparation_for_testnet_node(node, should_prepare)

    proposals = node.api.condenser.list_proposals(['alice'], 100, 'by_creator', 'ascending', 'all', 0)
    assert len(proposals) != 0


@run_for('testnet', 'live_mainnet')
def test_list_proposals_with_default_sixth_argument(node, should_prepare):
    preparation_for_testnet_node(node, should_prepare)

    proposals = node.api.condenser.list_proposals(['alice'], 100, 'by_creator', 'ascending', 'all')
    assert len(proposals) != 0


@run_for('testnet', 'live_mainnet')
def test_list_proposals_with_default_fifth_and_sixth_argument(node, should_prepare):
    preparation_for_testnet_node(node, should_prepare)

    proposals = node.api.condenser.list_proposals(['alice'], 100, 'by_creator', 'ascending')
    assert len(proposals) != 0


@run_for('testnet', 'live_mainnet')
def test_list_proposals_with_default_fourth_fifth_and_sixth_argument(node, should_prepare):
    preparation_for_testnet_node(node, should_prepare)

    proposals = node.api.condenser.list_proposals(['alice'], 100, 'by_creator')
    assert len(proposals) != 0


def preparation_for_testnet_node(node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.create_account('alice', hives=tt.Asset.Test(100), vests=tt.Asset.Test(100),
                              hbds=tt.Asset.Tbd(300))
        create_proposal(wallet, 'alice')
