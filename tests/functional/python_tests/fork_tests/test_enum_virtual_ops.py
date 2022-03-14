import test_tools as tt


def test_enum_virtual_ops(prepared_networks):
    # Test enum_virtual_ops for head block returns only virtual ops
    api_node = prepared_networks['Alpha'].node('ApiNode0')
    wallet = tt.Wallet(attach_to=api_node)

    account_name = 'gamma-1'
    wallet.api.create_account('initminer', account_name, '')
    api_node.wait_number_of_blocks(1)

    tt.logger.info('Checking there are only virtual operations in account_history.enum_virtual_ops...')
    for _ in range(10):
        # TRIGGER
        # We broadcast transactions (with non virtual operations).
        wallet.api.transfer_to_vesting('initminer', account_name, tt.Asset.Test(1))

        head_block = wallet.api.info()['head_block_number']
        block_to_check = head_block - 1

        # VERIFY
        # We check that query account_history_api.enum_virtual_ops returns only virtual operations (issue #139).
        assert_only_virtual_operations(api_node, block_to_check)


def assert_only_virtual_operations(node, block_to_check):
    response = node.api.account_history.enum_virtual_ops(block_range_begin=block_to_check, block_range_end=block_to_check+1, include_reversible=True)
    ops = response['ops']
    tt.logger.info(f"{[op['op']['type'] for op in ops]} in block {block_to_check}")

    for op in ops:
        virtual_op = op['virtual_op']
        assert virtual_op
