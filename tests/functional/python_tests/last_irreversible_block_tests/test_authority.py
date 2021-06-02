from test_tools import Account, logger, World
import random

def test_authority():
    with World() as world:
        net = world.create_network('Net')
        node = net.create_init_node()
        node.config.plugin.extend(['network_broadcast_api'])
        net.run()
 
        wallet = node.attach_wallet()
        wallet2 = node.attach_wallet()

        Alice = Account('tst-alice')
        alice_pub = 'TST7kvu8tJdFPxRXBr9bhGXu8cgbLZRyLJXoUbJyYLGUBTPim1NdG'
        alice_priv = '5JmitAWXUSh5VjXq2xCmiBexRyZUfBkb1ZTui9DofAzF6pTc8Bc'
        Auth = Account('tst-auth')
        auth_pub = 'TST5israys4kt6RTE6xB5wfYLVB38ujB5bBZ8CiaCTo3F8idFGexu'
        auth_priv = '5KDWAtFq4k5eFw6fs4wM5zq11HqrnsQpZi5xdBG1VmzD6PSfyZp'

        wallet.api.create_account_with_keys('initminer', Alice.name, "", alice_pub, alice_pub, alice_pub, alice_pub )
        wallet.api.create_account_with_keys('initminer', Auth.name, "", auth_pub, auth_pub, auth_pub, auth_pub )



        wallet.api.transfer('initminer', Alice.name, "1.000 TESTS", "test transfer")
        wallet.api.transfer_to_vesting('initminer', Alice.name, "1.000 TESTS")

        wallet.api.import_key(alice_priv)
        wallet.api.transfer(Alice.name, 'initminer', "0.001 TESTS", "this will work")

        wallet.api.update_account_auth_account(Alice.name, 'active', Auth.name, 1)
        wallet.api.transfer(Alice.name, 'initminer', "0.001 TESTS", "this will work but prints warning")



        wallet2.api.import_key(auth_priv)
        wallet2.api.import_key(alice_priv)
        wallet2.api.transfer(Alice.name, 'initminer', "0.001 TESTS", "this will NOT work")


     
