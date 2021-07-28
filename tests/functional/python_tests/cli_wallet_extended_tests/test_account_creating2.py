from test_tools import Account, logger, World, Asset
from utilities import check_keys

def test_account_creation2(world):
    init_node = world.create_init_node()
    init_node.run()

    wallet = init_node.attach_wallet()

    #**************************************************************
    response = wallet.api.list_accounts('a', 100)

    old_accounts_number = len(response['result'])

    logger.info('Waiting...')
    init_node.wait_number_of_blocks(18)
    
    #**************************************************************
    wallet.api.claim_account_creation('initminer', Asset.Test(0))

    #**************************************************************
    wallet.api.claim_account_creation_nonblocking('initminer', Asset.Test(0))

    #**************************************************************
    try:
        response = wallet.api.create_account_delegated('initminer', Asset.Test(3), '6.123456 VESTS', 'alicex', '{}')
    except Exception as e:
        message = str(e)
        found = message.find('Account creation with delegation is deprecated as of Hardfork 20')
        assert found != -1

    #**************************************************************
    key = 'TST8grZpsMPnH7sxbMVZHWEu1D26F3GwLW1fYnZEuwzT4Rtd57AER'
    response = wallet.api.create_account_with_keys('initminer', 'alice1', '{}', key, key, key, key)

    _operations = response['result']['operations']
    check_keys( _operations[0][1], key, key, key, key )

    #**************************************************************
    response = wallet.api.get_account('alice1')

    #**************************************************************
    try:
        key = 'TST8grZpsMPnH7sxbMVZHWEu1D26F3GwLW1fYnZEuwzT4Rtd57AER'
        response = wallet.api.create_account_with_keys_delegated('initminer', Asset.Test(4), '6.123456 VESTS', 'alicey', '{}', key, key, key, key)
    except Exception as e:
        message = str(e)
        found = message.find('Account creation with delegation is deprecated as of Hardfork 20')
        assert found != -1

    #**************************************************************
    key = 'TST8grZpsMPnH7sxbMVZHWEu1D26F3GwLW1fYnZEuwzT4Rtd57AER'
    wallet.api.create_funded_account_with_keys('initminer', 'alice2', Asset.Test(2), 'banana', '{}', key, key, key, key)

    #**************************************************************
    response = wallet.api.list_accounts('a', 100)
    assert old_accounts_number + 2 == len(response['result'])
