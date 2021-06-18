from test_tools import Asset, logger


def test_enum_virtual_ops(world_with_witnesses):
    world = world_with_witnesses

    # Test enum_virtual_ops for head block returns only virtual ops
    api_node = world.network('Alpha').node('ApiNode0')
    wallet = api_node.attach_wallet()

    account_name = 'gamma-1'
    wallet.api.create_account('initminer', account_name, '')
    api_node.wait_number_of_blocks(1)

    logger.info("Checking there are only virtual operations in account_history.enum_virtual_ops...")
    for _ in range(10):
        # TRIGGER
        # We broadcast transactions (with non virtual operations).
        wallet.api.transfer_to_vesting('initminer', account_name, Asset.Test(1))

        head_block = wallet.api.info()["result"]["head_block_number"]
        block_to_check = head_block - 1

        # VERIFY
        # We check that query account_history_api.enum_virtual_ops returns only virtual operations (issue #139).
        assert_only_virtual_operations(api_node, block_to_check)


def assert_only_virtual_operations(node, block_to_check):
    response = node.api.account_history.enum_virtual_ops(block_range_begin=block_to_check, block_range_end=block_to_check+1, include_reversible=True)
    ops = response["result"]["ops"]
    logger.info(str([op["op"]["type"] for op in ops]) + f" in block  {block_to_check}")

    for op in ops:
        virtual_op = op["virtual_op"]
        assert virtual_op>0
