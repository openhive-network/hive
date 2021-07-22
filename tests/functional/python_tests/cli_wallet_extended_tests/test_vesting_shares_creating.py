from test_tools import Account, logger, World

def test_delegate(world):
    init_node = world.create_init_node()
    init_node.run()

    wallet = init_node.attach_wallet()

    #**************************************************************
    response = wallet.api.create_account('initminer', 'alice', '{}')
    assert 'result' in response

    #**************************************************************
    response = wallet.api.transfer('initminer', 'alice', '200.000 TESTS', 'avocado')
    assert 'result' in response

    #**************************************************************
    response = wallet.api.transfer('initminer', 'alice', '100.000 TBD', 'banana')
    assert 'result' in response

    #**************************************************************
    response = wallet.api.transfer_to_vesting('initminer', 'alice', '50.000 TESTS')
    assert 'result' in response

    #**************************************************************
    response = wallet.api.create_account('alice', 'bob', '{}')
    assert 'result' in response

    #**************************************************************
    response = wallet.api.transfer('alice', 'bob', '50.000 TESTS', 'lemon')
    assert 'result' in response

    #**************************************************************
    response = wallet.api.transfer_to_vesting('alice', 'bob', '25.000 TESTS')
    assert 'result' in response

    #**************************************************************
    response = wallet.api.delegate_vesting_shares('alice', 'bob', '1.123456 VESTS')
    assert 'result' in response

    #**************************************************************
    response = wallet.api.get_account('alice')
    _result = response['result']

    assert _result['delegated_vesting_shares'] == '1.123456 VESTS'
    assert _result['balance'] == '125.000 TESTS'
    assert _result['hbd_balance'] == '100.000 TBD'

    #**************************************************************
    response = wallet.api.get_account('bob')
    _result = response['result']

    assert _result['received_vesting_shares'] == '1.123456 VESTS'
    assert _result['balance'] == '50.000 TESTS'
    assert _result['hbd_balance'] == '0.000 TBD'

    #**************************************************************
    response = wallet.api.delegate_vesting_shares_and_transfer('alice', 'bob', '1.000000 VESTS', '6.666 TBD', 'watermelon')
    assert 'result' in response

    #**************************************************************
    response = wallet.api.get_account('alice')
    _result = response['result']

    assert _result['delegated_vesting_shares'] == '1.123456 VESTS'
    assert _result['balance'] == '125.000 TESTS'
    assert _result['hbd_balance'] == '93.334 TBD'

    #**************************************************************
    response = wallet.api.get_account('bob')
    _result = response['result']

    assert _result['received_vesting_shares'] == '1.000000 VESTS'
    assert _result['balance'] == '50.000 TESTS'
    assert _result['hbd_balance'] == '6.666 TBD'

    #**************************************************************
    response = wallet.api.delegate_vesting_shares_nonblocking('bob', 'alice', '0.100000 VESTS')
    assert 'result' in response

    logger.info('Waiting...')
    init_node.wait_number_of_blocks(1)

    #**************************************************************
    response = wallet.api.get_account('alice')
    _result = response['result']

    assert _result['delegated_vesting_shares'] == '1.123456 VESTS'
    assert _result['received_vesting_shares'] == '0.100000 VESTS'
    assert _result['balance'] == '125.000 TESTS'
    assert _result['hbd_balance'] == '93.334 TBD'

    #**************************************************************
    response = wallet.api.get_account('bob')
    _result = response['result']

    assert _result['delegated_vesting_shares'] == '0.100000 VESTS'
    assert _result['received_vesting_shares'] == '1.000000 VESTS'
    assert _result['balance'] == '50.000 TESTS'
    assert _result['hbd_balance'] == '6.666 TBD'

    #**************************************************************
    response = wallet.api.delegate_vesting_shares_and_transfer_nonblocking('bob', 'alice', '0.100000 VESTS', '6.555 TBD', 'pear')
    assert 'result' in response

    logger.info('Waiting...')
    init_node.wait_number_of_blocks(1)

    #**************************************************************
    response = wallet.api.get_account('alice')
    _result = response['result']

    assert _result['delegated_vesting_shares'] == '1.123456 VESTS'
    assert _result['received_vesting_shares'] == '0.100000 VESTS'
    assert _result['balance'] == '125.000 TESTS'
    assert _result['hbd_balance'] == '99.889 TBD'

    #**************************************************************
    response = wallet.api.get_account('bob')
    _result = response['result']

    assert _result['delegated_vesting_shares'] == '0.100000 VESTS'
    assert _result['received_vesting_shares'] == '1.000000 VESTS'
    assert _result['balance'] == '50.000 TESTS'
    assert _result['hbd_balance'] == '0.111 TBD'

    #**************************************************************
    try:
        response = wallet.api.claim_reward_balance('initminer', '0.000 TESTS', '0.000 TBD', '0.000001 VESTS')
    except Exception as e:
        message = str(e)
        logger.info(message)
        found = message.find('Cannot claim that much VESTS')
        assert found != -1
