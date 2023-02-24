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

@pytest.mark.parametrize("step", (1, 2, 4, 8, 16, 32, 64))
@run_for('testnet')
def test_pagination(node: tt.InitNode, step: int):
    amount_of_transfers = 59
    amount_of_operations_from_account_creation = 5
    total_amount_of_operations = amount_of_transfers + amount_of_operations_from_account_creation

    wallet = tt.Wallet(attach_to=node)
    wallet.create_account('alice', hives=100, vests=100)

    with wallet.in_single_transaction():
        for x in range(amount_of_transfers):
            wallet.api.transfer('alice', 'null', tt.Asset.Test(1), f"transfer-{x}")
    response = node.api.account_history.get_account_history(account='alice', include_reversible=True)
    assert len(response['history']) == total_amount_of_operations

    ops_from_pagination = []
    for start in range(step - 1, total_amount_of_operations, step):
        output = node.api.account_history.get_account_history(account='alice', include_reversible=True, limit=step,
                                                              start=start)
        assert len(output['history']) > 0 or start == 0, f"history was empty for start={start}"
        ops_from_pagination += output['history']
        tt.logger.info(f"for start={start}, history has length of {len(output['history'])}")

    ops_from_pagination = list(sorted(ops_from_pagination, key=lambda x: x[0]))
    assert ops_from_pagination == response['history']
