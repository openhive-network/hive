from test_tools import Account, logger, World, Asset

def test_delegate(world):
    init_node = world.create_init_node()
    init_node.run()

    wallet = init_node.attach_wallet()

    wallet.api.create_account('initminer', 'alice', '{}')

    wallet.api.transfer('initminer', 'alice', Asset.Test(200), 'avocado')

    wallet.api.transfer('initminer', 'alice', Asset.Tbd(100), 'banana')

    wallet.api.transfer_to_vesting('initminer', 'alice', Asset.Test(50))

    wallet.api.create_account('alice', 'bob', '{}')

    wallet.api.transfer('alice', 'bob', Asset.Test(50), 'lemon')

    wallet.api.transfer_to_vesting('alice', 'bob', Asset.Test(25))

    wallet.api.delegate_vesting_shares('alice', 'bob', Asset.Vest(1.123456))

    response = wallet.api.get_account('alice')

    _result = response['result']
    assert _result['delegated_vesting_shares'] == Asset.Vest(1.123456)
    assert _result['balance'] == Asset.Test(125)
    assert _result['hbd_balance'] == Asset.Tbd(100)

    response = wallet.api.get_account('bob')

    _result = response['result']
    assert _result['received_vesting_shares'] == Asset.Vest(1.123456)
    assert _result['balance'] == Asset.Test(50)
    assert _result['hbd_balance'] == Asset.Tbd(0)

    wallet.api.delegate_vesting_shares_and_transfer('alice', 'bob', Asset.Vest(1), Asset.Tbd(6.666), 'watermelon')

    response = wallet.api.get_account('alice')

    _result = response['result']
    assert _result['delegated_vesting_shares'] == Asset.Vest(1.123456)
    assert _result['balance'] == Asset.Test(125)
    assert _result['hbd_balance'] == Asset.Tbd(93.334)

    response = wallet.api.get_account('bob')

    _result = response['result']
    assert _result['received_vesting_shares'] == Asset.Vest(1)
    assert _result['balance'] == Asset.Test(50)
    assert _result['hbd_balance'] == Asset.Tbd(6.666)

    wallet.api.delegate_vesting_shares_nonblocking('bob', 'alice', Asset.Vest(0.1))

    logger.info('Waiting...')
    init_node.wait_number_of_blocks(1)

    response = wallet.api.get_account('alice')

    _result = response['result']
    assert _result['delegated_vesting_shares'] == Asset.Vest(1.123456)
    assert _result['received_vesting_shares'] == Asset.Vest(0.1)
    assert _result['balance'] == Asset.Test(125)
    assert _result['hbd_balance'] == Asset.Tbd(93.334)

    response = wallet.api.get_account('bob')

    _result = response['result']
    assert _result['delegated_vesting_shares'] == Asset.Vest(0.1)
    assert _result['received_vesting_shares'] == Asset.Vest(1)
    assert _result['balance'] == Asset.Test(50)
    assert _result['hbd_balance'] == Asset.Tbd(6.666)

    wallet.api.delegate_vesting_shares_and_transfer_nonblocking('bob', 'alice', Asset.Vest(0.1), Asset.Tbd(6.555), 'pear')

    logger.info('Waiting...')
    init_node.wait_number_of_blocks(1)

    response = wallet.api.get_account('alice')

    _result = response['result']
    assert _result['delegated_vesting_shares'] == Asset.Vest(1.123456)
    assert _result['received_vesting_shares'] == Asset.Vest(0.1)
    assert _result['balance'] == Asset.Test(125)
    assert _result['hbd_balance'] == Asset.Tbd(99.889)

    response = wallet.api.get_account('bob')

    _result = response['result']
    assert _result['delegated_vesting_shares'] == Asset.Vest(0.1)
    assert _result['received_vesting_shares'] == Asset.Vest(1)
    assert _result['balance'] == Asset.Test(50)
    assert _result['hbd_balance'] == Asset.Tbd(0.111)

    try:
        response = wallet.api.claim_reward_balance('initminer', Asset.Test(0), Asset.Tbd(0), Asset.Vest(0.000001))
    except Exception as e:
        message = str(e)
        logger.info(message)
        found = message.find('Cannot claim that much VESTS')
        assert found != -1
