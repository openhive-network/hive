from test_tools import Account, logger, World
import random

def test_signing_with_authority():
    with World() as world:
        net = world.create_network('Net')
        node = net.create_init_node()
        node.config.plugin.extend([
            'network_broadcast_api', 'network_node_api', 'account_history_rocksdb', 'account_history_api'
        ])
        net.run()

        wallet = node.attach_wallet()
        wallet1 = node.attach_wallet()
        wallet2 = node.attach_wallet()
        wallet3 = node.attach_wallet()

        Alice = Account('tst-alice')
        Auth1 = Account('tst-auth1')
        Auth2 = Account('tst-auth2')
        Auth3 = Account('tst-auth3')

        wallet.api.create_account_with_keys('initminer', Alice.name, "", Alice.public_key, Alice.public_key, Alice.public_key, Alice.public_key )
        wallet.api.create_account_with_keys('initminer', Auth1.name, "", Auth1.public_key, Auth1.public_key, Auth1.public_key, Auth1.public_key )
        wallet.api.create_account_with_keys('initminer', Auth2.name, "", Auth2.public_key, Auth2.public_key, Auth2.public_key, Auth2.public_key )
        wallet.api.create_account_with_keys('initminer', Auth3.name, "", Auth3.public_key, Auth3.public_key, Auth3.public_key, Auth3.public_key )

        wallet.api.import_key(Alice.private_key)
        wallet1.api.import_key(Auth1.private_key)
        wallet2.api.import_key(Auth2.private_key)
        wallet3.api.import_key(Auth3.private_key)

        for account in [Alice, Auth1, Auth2, Auth3]:
            wallet.api.transfer('initminer', account.name, "1.000 TESTS", "test transfer")
            wallet.api.transfer_to_vesting('initminer', account.name, "1.000 TESTS")

        # TRIGGER and VERIFY
        wallet.api.transfer(Alice.name, 'initminer', "0.001 TESTS", "this will work")

        wallet.api.update_account_auth_account(Alice.name, 'active', Auth1.name, 1)
        # create cirtular authority dependency and test wallet behaves correctly
        wallet1.api.update_account_auth_account(Auth1.name, 'active', Auth2.name, 1)
        wallet1.api.update_account_auth_account(Auth1.name, 'active', Alice.name, 1)
        wallet2.api.update_account_auth_account(Auth2.name, 'active', Auth3.name, 1)

        wallet.api.transfer(Alice.name, 'initminer', "0.001 TESTS", "this will work and does not print warnings")

        # wallet1 and wallet2 can sign only with account authority
        wallet1.api.transfer(Alice.name, 'initminer', "0.001 TESTS", "this will work after fix")
        wallet2.api.transfer(Alice.name, 'initminer', "0.001 TESTS", "this will also work after fix")
        logger.info("successfully signed and broadcasted transaction with account authority")

        # assert siging with authodity does not work when dependency is to deep
        try:
            wallet3.api.transfer(Alice.name, 'initminer', "0.001 TESTS", "this will NOT work")
            assert False
        except Exception as e:
            assert 'Missing Active Authority' in str(e)
            logger.info(e)
