import test_tools as tt


def test_enum_virtual_ops(prepare_with_many_witnesses):
    # Test enum_virtual_ops for head block returns only virtual ops
    networks_builder = prepare_with_many_witnesses
    api_node = networks_builder.networks[0].node('FullApiNode0')
    wallet = tt.Wallet(attach_to=api_node)

    account_name = 'gamma-1'
    wallet.api.create_account('initminer', account_name, '')
    api_node.wait_number_of_blocks(1)

    tt.logger.info('Checking there are only virtual operations in account_history.enum_virtual_ops...')
    expected_results = [{"block": 111, "ops": ['producer_reward_operation']}]
    for i in range(112, 121):
      expected_results.append({"block": i, "ops": ['transfer_to_vesting_completed_operation', 'producer_reward_operation']})

    for _ in range(10):
        # TRIGGER
        # We broadcast transactions (with non virtual operations).

        # starting_block_number = wallet.api.info()['head_block_number']
        # tt.logger.info(f"staring block number = {starting_block_number}")

        wallet.api.transfer_to_vesting('initminer', account_name, tt.Asset.Test(1))
        head_block = wallet.api.info()['head_block_number']
        # tt.logger.info(f"head block number = {head_block}")
        block_to_check = head_block - 1

        # VERIFY
        # We check that query account_history_api.enum_virtual_ops returns only virtual operations (issue #139).
        assert_only_virtual_operations(api_node, block_to_check, expected_results[_])


def assert_only_virtual_operations(node, block_to_check, expected_results):
    response = node.api.account_history.enum_virtual_ops(block_range_begin=block_to_check, block_range_end=block_to_check+1, include_reversible=True)
    ops = response['ops']
    tt.logger.info(f"{[op['op']['type'] for op in ops]} in block {block_to_check}")
    tt.logger.info(f"expected results for block: {block_to_check}: {expected_results}")

    results = [op['op']['type'] for op in ops]
    assert results == expected_results['ops']

    for op in ops:
        virtual_op = op['virtual_op']
        assert virtual_op
