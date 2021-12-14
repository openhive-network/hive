from pathlib import Path
import re
import subprocess
import time

from test_tools import Asset, communication, logger, Hivemind, World, Wallet

def test_hivemind_start_sync(world: World):
    node_test = world.create_init_node()
    node_test.run()
    wallet = Wallet(attach_to=node_test)
    hivemind = Hivemind(
        database_name='hivemind_pyt',
        database_user='dev',
        database_password='devdevdev',
    )
    hivemind.run(sync_with=node_test)
    time.sleep(5)
    # message = communication.request(url='http://localhost:8080', message={"jsonrpc":"2.0","id":0,"method":"hive.db_head_state","params":{}})
    print()


def test_vote_bug(world:World):
    node_test = world.create_init_node()
    node_test.run()
    wallet = Wallet(attach_to=node_test)

    hivemind = Hivemind(
        database_name='hivemind_pyt',
        database_user='dev',
        database_password='devdevdev',
    )

    hivemind.run(sync_with=node_test)

    wallet.api.create_account('initminer', 'alice', '{}')
    wallet.api.create_account('initminer', 'bob', '{}')
    wallet.api.transfer('initminer', 'alice', Asset.Tbd(788.543), 'avocado')
    wallet.api.transfer('initminer', 'bob', Asset.Tbd(788.543), 'banana')
    wallet.api.transfer_to_vesting('initminer', 'alice', Asset.Test(100))
    wallet.api.transfer_to_vesting('initminer', 'bob', Asset.Test(100))
    wallet.api.post_comment('alice', 'hello-world2', '', 'xyz2', 'something about world2', 'just nothing2', '{}')

    node_test.wait_for_block_with_number(30)
    wallet.api.vote('bob', 'alice', 'hello-world2', 99)

    node_test.wait_number_of_blocks(3)





def test_accounts(world: World):
    node_test = world.create_init_node()
    node_test.run()
    wallet = Wallet(attach_to=node_test)

    wallet.api.create_account('initminer', 'alice', '{}')
    wallet.api.create_account('initminer', 'bob', '{}')
    wallet.api.create_account('initminer', 'carol', '{}')
    wallet.api.create_account('initminer', 'dan', '{}')

    wallet.api.transfer_to_vesting('initminer', 'bob', Asset.Test(10))

    wallet.api.post_comment('bob', 'hello-world', '', 'xyz', 'something about world', 'just nothing', '{}')

    wallet.api.transfer('initminer', 'bob', Asset.Tbd(788.543), 'avocado')

    wallet.api.create_proposal('bob', 'bob', '2031-01-01T00:00:00', '2031-06-01T00:00:00', Asset.Tbd(111), 'this is proposal', 'hello-world')

    with wallet.in_single_transaction(broadcast=False) as transaction:
        wallet.api.transfer('bob', 'alice', Asset.Tbd(199.148), 'banana')
        wallet.api.transfer('bob', 'alice', Asset.Tbd(100.001), 'cherry')
        wallet.api.transfer('initminer', 'alice', Asset.Tbd(1), 'aloes')
        wallet.api.transfer('initminer', 'carol', Asset.Tbd(199.001), 'pumpkin')
        wallet.api.transfer('initminer', 'dan', Asset.Tbd(198.002), 'beetroot')
        wallet.api.transfer_to_vesting('initminer', 'carol', Asset.Test(100))
        wallet.api.transfer_to_vesting('initminer', 'alice', Asset.Test(100))
        wallet.api.transfer_to_vesting('initminer', 'dan', Asset.Test(100))

    _result_trx_response = transaction.get_response()

    response = wallet.api.sign_transaction(_result_trx_response)

    assert len(response['operations']) == 8

    with wallet.in_single_transaction(broadcast=False) as transaction:
        wallet.api.post_comment('alice', 'hello-world2', '', 'xyz2', 'something about world2', 'just nothing2', '{}')
        wallet.api.post_comment('carol', 'hello-world3', '', 'xyz3', 'something about world3', 'just nothing3', '{}')
        wallet.api.post_comment('dan', 'hello-world4', '', 'xyz4', 'something about world4', 'just nothing4', '{}')
        wallet.api.vote('carol', 'alice', 'hello-world2', 99)
        wallet.api.vote('dan', 'carol', 'hello-world3', 98)
        wallet.api.vote('alice', 'dan', 'hello-world4', 97)

    _result_trx_response = transaction.get_response()

    response = wallet.api.sign_transaction(_result_trx_response)

    assert len(response['operations']) == 6

    hivemind = Hivemind(
        database_name='hivemind_pyt',
        database_user='dev',
        database_password='devdevdev',
    )

    hivemind.run(sync_with=node_test)

    message1 = communication.request(url='http://localhost:8080', message={"jsonrpc":"2.0","id":0,"method":"bridge.get_post","params":{"author":"dan", "permlink":"image-server-cluster-development-and-maintenance"}})
    message2 = communication.request(url='http://localhost:8080', message={"jsonrpc":"2.0","id":0,"method":"bridge.get_account_posts","params":{"account":"dan", "sort":"replies"}})

    print()
