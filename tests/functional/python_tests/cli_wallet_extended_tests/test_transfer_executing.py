from test_tools import Account, logger, World, Asset

def test_transfer(world):
    init_node = world.create_init_node()
    init_node.run()

    wallet = init_node.attach_wallet()

    #**************************************************************
    response = wallet.api.create_account('initminer', 'newaccount', '{}')
    assert 'result' in response

    #**************************************************************
    response = wallet.api.create_account('initminer', 'newaccount2', '{}')
    assert 'result' in response

    #**************************************************************
    response = wallet.api.transfer_to_vesting('initminer', 'newaccount', Asset.Test(100))
    assert 'result' in response

    #**************************************************************
    response = wallet.api.get_account('newaccount')
    _result = response['result']

    assert _result['balance'] == Asset.Test(0)
    assert _result['hbd_balance'] == Asset.Tbd(0)
    assert _result['savings_balance'] == Asset.Test(0)
    assert _result['savings_hbd_balance'] == Asset.Tbd(0)
    assert _result['vesting_shares'] != '0.000000 VESTS'

    #**************************************************************
    response = wallet.api.transfer('initminer', 'newaccount', '5.432 TESTS', 'banana')
    assert 'result' in response

    #**************************************************************
    response = wallet.api.transfer('initminer', 'newaccount', '9.169 TBD', 'banana2')
    assert 'result' in response

    #**************************************************************
    response = wallet.api.transfer_to_savings('initminer', 'newaccount', '15.432 TESTS', 'pomelo')
    assert 'result' in response

    #**************************************************************
    response = wallet.api.transfer_to_savings('initminer', 'newaccount', '19.169 TBD', 'pomelo2')
    assert 'result' in response

    #**************************************************************
    response = wallet.api.get_account('newaccount')
    _result = response['result']

    assert _result['balance'] == '5.432 TESTS'
    assert _result['hbd_balance'] == '9.169 TBD'
    assert _result['savings_balance'] == '15.432 TESTS'
    assert _result['savings_hbd_balance'] == '19.169 TBD'

    #**************************************************************
    response = wallet.api.transfer_from_savings('newaccount', 7, 'newaccount2', '0.001 TESTS', 'kiwi')
    assert 'result' in response

    #**************************************************************
    response = wallet.api.transfer_from_savings('newaccount', 8, 'newaccount2', '0.001 TBD', 'kiwi2')
    assert 'result' in response

    #**************************************************************
    response = wallet.api.get_account('newaccount')
    _result = response['result']

    assert _result['balance'] == '5.432 TESTS'
    assert _result['hbd_balance'] == '9.169 TBD'
    assert _result['savings_balance'] == '15.431 TESTS'
    assert _result['savings_hbd_balance'] == '19.168 TBD'

    #**************************************************************
    response = wallet.api.transfer_nonblocking('newaccount', 'newaccount2', '0.100 TESTS', 'mango')

    #**************************************************************
    response = wallet.api.transfer_nonblocking('newaccount', 'newaccount2', '0.200 TBD', 'mango2')
    assert 'result' in response

    #**************************************************************
    response = wallet.api.transfer_to_vesting_nonblocking('initminer', 'newaccount', Asset.Test(100))
    assert 'result' in response

    logger.info('Waiting...')
    init_node.wait_number_of_blocks(1)

    #**************************************************************
    response = wallet.api.get_account('newaccount')
    _result = response['result']

    assert _result['balance'] == '5.332 TESTS'
    assert _result['hbd_balance'] == '8.969 TBD'
    assert _result['savings_balance'] == '15.431 TESTS'
    assert _result['vesting_shares'] != '0.000000 VESTS'

    #**************************************************************
    response = wallet.api.cancel_transfer_from_savings('newaccount', 7)
    assert 'result' in response

    #**************************************************************
    response = wallet.api.get_account('newaccount')
    _result = response['result']

    assert _result['savings_balance'] == '15.432 TESTS'
