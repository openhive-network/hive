# This is a regression test for problem described in following issue:
# https://gitlab.syncad.com/hive/hive/-/issues/113

from test_tools import Account, logger


def test_getting_key_references_of_claimed_created_account(world):
    init_node = world.create_init_node()
    init_node.run()

    wallet = init_node.attach_wallet()

    logger.info('Waiting until initminer will be able to create account...')
    init_node.wait_number_of_blocks(30)

    account = Account('alice')
    key = account.public_key
    wallet.api.claim_account_creation('initminer', '0.000 TESTS')
    wallet.api.create_funded_account_with_keys(
        'initminer', account.name, '0.000 TESTS', 'memo', '{}', key, key, key, key
    )

    response = init_node.api.condenser.get_key_references([account.public_key])
    assert response['result'] == [[account.name]]
