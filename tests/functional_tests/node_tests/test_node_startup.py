import pytest

from test_tools import Account


def test_init_node_startup(world):
    init_node = world.create_init_node()
    init_node.run()
    init_node.wait_for_block_with_number(1)


def test_startup_timeout(world):
    node = world.create_witness_node(witnesses=[])
    with pytest.raises(TimeoutError):
        node.run(timeout=2)


def test_replay_from_other_node_block_log(world):
    init_node = world.create_init_node()
    init_node.run()

    make_transaction_for_test(init_node)
    generate_blocks(init_node, 100)  # To make sure, that block with test operation will be stored in block log
    init_node.close()

    replaying_node = world.create_api_node()
    replaying_node.run(replay_from=init_node.get_block_log(), wait_for_live=False)
    assert_that_transaction_for_test_has_effect(replaying_node)


def make_transaction_for_test(node):
    wallet = node.attach_wallet()
    wallet.api.create_account('initminer', 'alice', '{}')


def assert_that_transaction_for_test_has_effect(node):
    response = node.api.database.find_accounts(accounts=['alice'], delayed_votes_active=False)
    assert response['result']['accounts'][0]['name'] == 'alice'


# TODO: This function can be moved to Node class, but
#       first we need to learn how it really works.
def generate_blocks(node, number_of_blocks):
    node.api.debug_node.debug_generate_blocks(
        debug_key=Account('initminer').private_key,
        count=number_of_blocks,
        skip=0,
        miss_blocks=0,
        edit_if_needed=True
    )
