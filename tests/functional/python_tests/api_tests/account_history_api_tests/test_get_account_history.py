import pytest
import test_tools as tt
from hive_local_tools import run_for


@run_for('testnet')
def test_get_empty_history(node):
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account('alice')
    response = node.api.account_history.get_account_history(account='alice')
    assert len(response['history']) == 0


@pytest.mark.parametrize('include_reversible', (
        True, False,
))
@run_for('testnet')
def test_check_for_newly_created_history_operations(node, include_reversible):
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account('alice', hives=100)
    if not include_reversible:
        node.wait_for_irreversible_block()
    response = node.api.account_history.get_account_history(account='alice', include_reversible=include_reversible)
    assert len(response['history']) > 0


@run_for('testnet')
def test_filter_only_transfer_ops(node):
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account('alice', hives=100)
    response = node.api.account_history.get_account_history(account='alice', include_reversible=True,
                                                            operation_filter_low=4)
    assert len(response['history']) == 1


@run_for('testnet')
def test_pagination(node):
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account('alice', hives=100, vests=100)

    with wallet.in_single_transaction():
        for x in range(50):
            wallet.api.transfer('alice', 'null', tt.Asset.Test(1), f"transfer-{x}")
    response = node.api.account_history.get_account_history(account='alice', include_reversible=True)
    ops_from_pagination = []
    limit = 5
    start = 5
    for x in range(int((len(response['history'])/limit))):
        output = node.api.account_history.get_account_history(account='alice', include_reversible=True, limit=limit,
                                                              start=start)
        ops_from_pagination += output['history']
        start += limit

    assert ops_from_pagination == response['history']
