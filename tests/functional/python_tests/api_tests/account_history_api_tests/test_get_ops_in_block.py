import pytest
import test_tools as tt
from hive_local_tools import run_for


@run_for('testnet')
def test_default_args_value(node):
    node.api.account_history.get_ops_in_block()


@pytest.mark.parametrize('only_virtual, number_of_ops', (
        (False, 3),
        (True, 2)
))
@run_for('testnet')
def test_filter_virtual_ops(node, only_virtual, number_of_ops):
    wallet = tt.Wallet(attach_to=node)
    block_number = wallet.create_account('alice')['block_num']
    response = node.api.account_history.get_ops_in_block(block_num=block_number,
                                                         only_virtual=only_virtual,
                                                         include_reversible=True)
    assert len(response['ops']) == number_of_ops


@pytest.mark.parametrize('include_reversible, comparison_type', (
        (False, '__eq__'),
        (True, '__gt__')
))
@run_for('testnet')
def test_get_operations_in_block_with_and_without_reversible(node, include_reversible, comparison_type):
    response = node.api.account_history.get_ops_in_block(block_num=1,
                                                         only_virtual=False,
                                                         include_reversible=include_reversible)
    assert getattr(len(response['ops']), comparison_type)(0)


@run_for('testnet')
def test_get_ops_in_non_existent_block(node):
    node.api.account_history.get_ops_in_block(block_num=-1)
