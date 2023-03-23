import itertools

import pytest
import test_tools as tt
from hive_local_tools import run_for


@run_for('testnet')
def send_transfers_to_vesting_from_initminer_to_alice(wallet: tt.Wallet, *, amount: int):
    for x in range(amount):
        wallet.api.transfer_to_vesting('initminer', 'alice', tt.Asset.Test(1))


@run_for('testnet', enable_plugins=['account_history_api'])
def test_exceed_block_range(node):
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.account_history.enum_virtual_ops(block_range_begin=1, block_range_end=2002)


@run_for('testnet', enable_plugins=['account_history_api'])
def test_filter_only_hardfork_operations(node):
    block_to_start = node.get_last_block_number()
    node.wait_number_of_blocks(1)
    response = node.api.account_history.enum_virtual_ops(
        block_range_begin=block_to_start,
        block_range_end=block_to_start + 1,
        include_reversible=True,
        filter=0x000400
    )
    number_of_hardforks = int(node.api.database.get_config()['HIVE_BLOCKCHAIN_HARDFORK_VERSION'].split('.')[1])
    assert len(response['ops']) == number_of_hardforks


@run_for('testnet', enable_plugins=['account_history_api'])
def test_find_irreversible_operations(node):
    block_to_start = node.get_last_block_number()
    # wait for the block with the transaction to become irreversible
    node.wait_for_irreversible_block()
    end_block = node.get_last_block_number()
    response = node.api.account_history.enum_virtual_ops(
        block_range_begin=block_to_start,
        block_range_end=end_block,
    )
    assert len(response['ops']) > 0


@run_for('testnet', enable_plugins=['account_history_api'])
def test_find_newly_created_virtual_op(node):
    wallet = tt.Wallet(attach_to=node)
    block_to_start = node.get_last_block_number()
    wallet.create_account('alice')
    # transfer_to_vesting indicates transfer_to_vesting_completed virtual operation
    transaction = wallet.api.transfer_to_vesting('initminer', 'alice', tt.Asset.Test(100))
    # block_range_end arg takes block number exclusively that's why wait 1 more block
    node.wait_number_of_blocks(1)
    end_block = node.get_last_block_number()
    response = node.api.account_history.enum_virtual_ops(
        block_range_begin=block_to_start,
        block_range_end=end_block,
        include_reversible=True,
        filter=0x8000000,
    )
    assert len(response['ops']) == 1
    assert response['ops'][0]['trx_id'] == transaction['transaction_id']


@run_for('testnet', enable_plugins=['account_history_api'])
def test_find_reversible_virtual_operations(node):
    block_to_start = node.get_last_block_number()
    node.wait_number_of_blocks(1)
    response = node.api.account_history.enum_virtual_ops(
        block_range_begin=block_to_start,
        block_range_end=block_to_start + 1,
        include_reversible=True,
    )
    assert len(response['ops']) > 0


@run_for('testnet', enable_plugins=['account_history_api'])
def test_grouping_by_block(node):
    node.wait_number_of_blocks(3)
    block_to_start = node.get_last_block_number()
    wallet = tt.Wallet(attach_to=node)

    accounts_to_create = 20
    # create many accounts in different blocks
    for x in range(accounts_to_create):
        wallet.create_account(f'account-{x}')

    # block_range_end arg takes block number exclusively that's why wait 1 more block
    node.wait_number_of_blocks(1)
    end_block = node.get_last_block_number()
    response = node.api.account_history.enum_virtual_ops(
        block_range_begin=block_to_start,
        block_range_end=end_block,
        group_by_block=True,
        include_reversible=True,
        filter=0x40000000,
    )

    assert len(response['ops']) == 0
    assert len(response['ops_by_block']) == accounts_to_create

    # check if transactions are in blocks after each other
    for x in range(1, len(response['ops_by_block'])):
        assert response['ops_by_block'][x-1]['block'] == response['ops_by_block'][x]['block']-1


@pytest.mark.parametrize('group_by_block, include_reversible', itertools.product((True, False), (True, False)))
@run_for('testnet', enable_plugins=['account_history_api'])
def test_limit(node, group_by_block: bool, include_reversible: bool):

    node.wait_number_of_blocks(1)
    block_to_start = node.get_last_block_number()
    wallet = tt.Wallet(attach_to=node)
    wallet.create_accounts(1100)

    # block_range_end arg takes block number exclusively that's why wait 1 more block
    node.wait_number_of_blocks(1)
    end_block = node.get_last_block_number()

    if not include_reversible:
        node.wait_for_irreversible_block()

    response = node.api.account_history.enum_virtual_ops(
        block_range_begin=block_to_start,
        block_range_end=end_block,
        include_reversible=include_reversible,
        group_by_block=group_by_block,
        operation_begin=0,
        limit=2
    )

    amount_of_returned_operations: int = 0
    if group_by_block:
        for block in response['ops_by_block']:
            amount_of_returned_operations += len(block['ops'])
    else:
        amount_of_returned_operations += len(response['ops'])

    assert amount_of_returned_operations == 2


@pytest.mark.parametrize('group_by_block, key', (
        (False, 'ops'),
        (True, 'ops_by_block')
))
@run_for('testnet', enable_plugins=['account_history_api'])
def test_list_vops_partly_in_irreversible_and_partly_in_reversible_blocks(node, group_by_block, key):
    node.wait_number_of_blocks(1)
    block_to_start = node.get_last_block_number()
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account('alice')

    send_transfers_to_vesting_from_initminer_to_alice(wallet, amount=3)

    # make ops irreversible
    node.wait_for_irreversible_block()

    send_transfers_to_vesting_from_initminer_to_alice(wallet, amount=3)

    # block_range_end arg takes block number exclusively that's why wait 1 more block
    node.wait_number_of_blocks(1)
    end_block = node.get_last_block_number()
    limit = 5
    response = node.api.account_history.enum_virtual_ops(block_range_begin=block_to_start, block_range_end=end_block,
                                                         limit=limit, filter=0x8000000, include_reversible=True,
                                                         group_by_block=group_by_block)

    assert len(response[key]) == limit


@pytest.mark.parametrize('group_by_block', (False, True))
@run_for('testnet', enable_plugins=['account_history_api'])
def test_no_virtual_operations(node, group_by_block: bool):
    node.wait_number_of_blocks(5)
    # check default values of block_range_begin/block_range_end too
    response = node.api.account_history.enum_virtual_ops(
        group_by_block=group_by_block,
    )
    assert len(response['ops']) == 0
    assert len(response['ops_by_block']) == 0
    assert response['next_block_range_begin'] == 0
    assert response['next_operation_begin'] == 0


@run_for('testnet', enable_plugins=['account_history_api'])
def test_number_of_producer_reward_ops(node):
    node.wait_number_of_blocks(3)
    block_to_start = node.get_last_block_number()
    blocks_to_wait = 5
    node.wait_number_of_blocks(blocks_to_wait)
    end_block = node.get_last_block_number()
    response = node.api.account_history.enum_virtual_ops(
        block_range_begin=block_to_start,
        block_range_end=end_block,
        include_reversible=True,
    )
    assert len(response['ops']) == blocks_to_wait


@run_for('testnet', enable_plugins=['account_history_api'])
def test_pagination(node):
    node.wait_number_of_blocks(1)
    block_to_start = node.get_last_block_number()
    wallet = tt.Wallet(attach_to=node)
    wallet.create_accounts(15)
    # block_range_end arg takes block number exclusively that's why wait 1 more block
    node.wait_number_of_blocks(1)
    end_block = node.get_last_block_number()
    node.wait_for_irreversible_block()
    response = node.api.account_history.enum_virtual_ops(
        block_range_begin=block_to_start,
        block_range_end=end_block,
    )
    number_of_ops = len(response['ops'])
    next_op_id = response['ops'][0]['operation_id']
    ops_from_pagination = []
    for x in range(number_of_ops):
        output = node.api.account_history.enum_virtual_ops(
            block_range_begin=block_to_start,
            block_range_end=end_block,
            operation_begin=next_op_id,
            limit=1,
        )
        next_op_id = output['next_operation_begin']
        ops_from_pagination += output['ops']
    assert ops_from_pagination == response['ops']


@run_for('testnet', enable_plugins=['account_history_api'])
def test_same_block_range_begin_and_end(node):
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.account_history.enum_virtual_ops(block_range_begin=1, block_range_end=1)
