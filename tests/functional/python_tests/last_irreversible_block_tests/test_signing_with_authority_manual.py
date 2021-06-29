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
        Bob = Account('tst-bob')

        wallet.api.create_account_with_keys('initminer', Alice.name, "", Alice.public_key, Alice.public_key, Alice.public_key, Alice.public_key )
        wallet.api.create_account_with_keys('initminer', Auth1.name, "", Auth1.public_key, Auth1.public_key, Auth1.public_key, Auth1.public_key )
        wallet.api.create_account_with_keys('initminer', Auth2.name, "", Auth2.public_key, Auth2.public_key, Auth2.public_key, Auth2.public_key )
        wallet.api.create_account_with_keys('initminer', Bob.name, "", Bob.public_key, Bob.public_key, Bob.public_key, Bob.public_key )

        wallet.api.import_key(Alice.private_key)
        wallet1.api.import_key(Auth1.private_key)
        wallet2.api.import_key(Auth2.private_key)
        wallet3.api.import_key(Bob.private_key)

        for account in [Alice, Auth1, Auth2, Bob]:
            wallet.api.transfer('initminer', account.name, "1.000 TESTS", "test transfer")
            wallet.api.transfer_to_vesting('initminer', account.name, "1.000 TESTS")

        # TRIGGER and VERIFY
        wallet.api.transfer(Alice.name, 'initminer', "0.001 TESTS", "this will work")
        logger.info(f"Transaction signed with default key")

        wallet.api.update_account_auth_account(Alice.name, 'active', Auth1.name, 1)
        # create cirtular authority dependency and test wallet behaves correctly
        wallet1.api.update_account_auth_account(Auth1.name, 'active', Auth2.name, 1)
        wallet1.api.update_account_auth_account(Auth1.name, 'active', Alice.name, 1)

        logger.info(f"Manual set authority to {Auth1.name}")
        wallet1.api.set_authorities({"active": [Auth1.name]})
        wallet1.api.transfer(Alice.name, 'initminer', "0.001 TESTS", "this WILL work because correct authority is selected manually")
        logger.info(f"Transaction signed with foreign authority {Auth1.name}")

        try:
            logger.info(f"Manual set authority to {Alice.name} (expected fail)")
            wallet1.api.set_authorities({"active": [Alice.name]})
            wallet1.api.transfer(Alice.name, 'initminer', "0.001 TESTS", "this WILL NOT work because tst-alice key is not imported into wallet")
            assert False
        except Exception as e:
            assert 'Missing Active Authority' in str(e)
            logger.info(f"Transaction cannot be signed with {Alice.name} authority because keys are not present")
        

        logger.info(f"Manual set authority to {Auth2.name}")
        wallet2.api.set_authorities({"active": [Auth2.name]})
        wallet2.api.transfer(Alice.name, 'initminer', "0.001 TESTS", "this WILL work")
        logger.info(f"Transaction signed with foreign authority {Auth2.name}")

        try:
            logger.info(f"Manual set authority to {Bob.name} (expected fail)")
            wallet3.api.set_authorities({"active": [Bob.name]})
            wallet3.api.transfer(Alice.name, 'initminer', "0.001 TESTS", "this WILL NOT work because tst-bob key is not correct authority for this transaction")
            assert False
        except Exception as e:
            assert 'Missing Active Authority' in str(e)
            logger.info(f"Transaction cannot be signed with {Bob.name} authority because this is not correct authority for this transaction")

        try:
            logger.info(f"Manual set authority to {Bob.name} (expected fail) even though required keys are available in wallet")
            wallet3.api.import_key(Alice.private_key)
            wallet3.api.set_authorities({"active": [Bob.name]})
            wallet3.api.transfer(Alice.name, 'initminer', "0.001 TESTS", "this WILL NOT work because tst-bob key is not correct authority for this transaction")
            assert False
        except Exception as e:
            assert 'Missing Active Authority' in str(e)
            logger.info(f"Transaction cannot be signed with {Bob.name} authority because this is not correct authority for this transaction")

        logger.info(f"Back to automatic authority selection, then sign and broadcast")
        wallet3.api.set_authorities(None)
        wallet3.api.transfer(Alice.name, 'initminer', "0.001 TESTS", "this WILL with default authority selection")
        logger.info(f"Transaction signed with foreign authority (automatic selection)")
