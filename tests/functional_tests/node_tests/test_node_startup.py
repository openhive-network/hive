import pytest

from test_tools import Account, Wallet


def test_init_node_startup(world):
    init_node = world.create_init_node()
    init_node.run()
    init_node.wait_for_block_with_number(1)


def test_startup_timeout(world):
    node = world.create_witness_node(witnesses=[])
    with pytest.raises(TimeoutError):
        node.run(timeout=2)


def test_loading_own_snapshot(world):
    init_node = world.create_init_node()
    init_node.run()

    make_transaction_for_test(init_node)
    generate_blocks(init_node, 100)  # To make sure, that block with test operation will be stored in block log
    snapshot = init_node.dump_snapshot(close=True)

    # Node is not waiting for live here, because of blocks generation behavior.
    # After N blocks generation, node will stop blocks production for N * 3s.
    init_node.run(load_snapshot_from=snapshot, wait_for_live=False)
    assert_that_transaction_for_test_has_effect(init_node)


def test_loading_snapshot_from_other_node(world):
    init_node = world.create_init_node()
    init_node.run()

    make_transaction_for_test(init_node)
    generate_blocks(init_node, 100)  # To make sure, that block with test operation will be stored in block log
    snapshot = init_node.dump_snapshot()

    loading_node = world.create_api_node()
    loading_node.run(load_snapshot_from=snapshot, wait_for_live=False)
    assert_that_transaction_for_test_has_effect(loading_node)


def test_loading_snapshot_from_custom_path(world):
    init_node = world.create_init_node()
    init_node.run()

    make_transaction_for_test(init_node)
    generate_blocks(init_node, 100)  # To make sure, that block with test operation will be stored in block log
    snapshot = init_node.dump_snapshot()

    loading_node = world.create_api_node()
    loading_node.run(load_snapshot_from=snapshot.get_path(), wait_for_live=False)
    assert_that_transaction_for_test_has_effect(loading_node)


def test_replay_from_other_node_block_log(world):
    init_node = world.create_init_node()
    init_node.run()

    make_transaction_for_test(init_node)
    generate_blocks(init_node, 100)  # To make sure, that block with test operation will be stored in block log
    init_node.close()

    replaying_node = world.create_api_node()
    replaying_node.run(replay_from=init_node.get_block_log(), wait_for_live=False)
    assert_that_transaction_for_test_has_effect(replaying_node)


def test_replay_until_specified_block(world):
    init_node = world.create_init_node()
    init_node.run()
    generate_blocks(init_node, 100)
    init_node.close()

    replaying_node = world.create_api_node()
    replaying_node.run(replay_from=init_node.get_block_log(), stop_at_block=50, wait_for_live=False)
    assert replaying_node.get_last_block_number() == 50


def test_replay_from_external_block_log(world):
    init_node = world.create_init_node()
    init_node.run()
    generate_blocks(init_node, 100)
    init_node.close()

    external_block_log_path = init_node.get_block_log().get_path()

    replaying_node = world.create_api_node()
    replaying_node.run(replay_from=external_block_log_path, stop_at_block=50, wait_for_live=False)
    assert replaying_node.get_last_block_number() == 50


def test_exit_before_synchronization(world):
    init_node = world.create_init_node()
    init_node.run(exit_before_synchronization=True)
    assert not init_node.is_running()


def make_transaction_for_test(node):
    wallet = Wallet(attach_to=node)
    wallet.api.create_account('initminer', 'alice', '{}')


def assert_that_transaction_for_test_has_effect(node):
    response = node.api.database.find_accounts(accounts=['alice'], delayed_votes_active=False)
    assert response['accounts'][0]['name'] == 'alice'


def generate_blocks(node, number_of_blocks):
    node.api.debug_node.debug_generate_blocks(
        debug_key=Account('initminer').private_key,
        count=number_of_blocks,
        skip=0,
        miss_blocks=0,
        edit_if_needed=True
    )
