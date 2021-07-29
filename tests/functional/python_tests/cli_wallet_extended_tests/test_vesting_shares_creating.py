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

    wallet.api.delegate_vesting_shares('alice', 'bob', '1.123456 VESTS')

    response = wallet.api.get_account('alice')

    _result = response['result']
    assert _result['delegated_vesting_shares'] == '1.123456 VESTS'
    assert _result['balance'] == Asset.Test(125)
    assert _result['hbd_balance'] == Asset.Tbd(100)

    response = wallet.api.get_account('bob')

    _result = response['result']
    assert _result['received_vesting_shares'] == '1.123456 VESTS'
    assert _result['balance'] == Asset.Test(50)
    assert _result['hbd_balance'] == Asset.Tbd(0)

    wallet.api.delegate_vesting_shares_and_transfer('alice', 'bob', '1.000000 VESTS', '6.666 TBD', 'watermelon')

    response = wallet.api.get_account('alice')

    _result = response['result']
    assert _result['delegated_vesting_shares'] == '1.123456 VESTS'
    assert _result['balance'] == Asset.Test(125)
    assert _result['hbd_balance'] == '93.334 TBD'

    response = wallet.api.get_account('bob')

    _result = response['result']
    assert _result['received_vesting_shares'] == '1.000000 VESTS'
    assert _result['balance'] == Asset.Test(50)
    assert _result['hbd_balance'] == '6.666 TBD'

    wallet.api.delegate_vesting_shares_nonblocking('bob', 'alice', '0.100000 VESTS')

    logger.info('Waiting...')
    init_node.wait_number_of_blocks(1)

    response = wallet.api.get_account('alice')

    _result = response['result']
    assert _result['delegated_vesting_shares'] == '1.123456 VESTS'
    assert _result['received_vesting_shares'] == '0.100000 VESTS'
    assert _result['balance'] == Asset.Test(125)
    assert _result['hbd_balance'] == '93.334 TBD'

    response = wallet.api.get_account('bob')

    _result = response['result']
    assert _result['delegated_vesting_shares'] == '0.100000 VESTS'
    assert _result['received_vesting_shares'] == '1.000000 VESTS'
    assert _result['balance'] == Asset.Test(50)
    assert _result['hbd_balance'] == '6.666 TBD'

    wallet.api.delegate_vesting_shares_and_transfer_nonblocking('bob', 'alice', '0.100000 VESTS', '6.555 TBD', 'pear')

    logger.info('Waiting...')
    init_node.wait_number_of_blocks(1)

    response = wallet.api.get_account('alice')

    _result = response['result']
    assert _result['delegated_vesting_shares'] == '1.123456 VESTS'
    assert _result['received_vesting_shares'] == '0.100000 VESTS'
    assert _result['balance'] == Asset.Test(125)
    assert _result['hbd_balance'] == '99.889 TBD'

    response = wallet.api.get_account('bob')

    _result = response['result']
    assert _result['delegated_vesting_shares'] == '0.100000 VESTS'
    assert _result['received_vesting_shares'] == '1.000000 VESTS'
    assert _result['balance'] == Asset.Test(50)
    assert _result['hbd_balance'] == '0.111 TBD'

    try:
        response = wallet.api.claim_reward_balance('initminer', Asset.Test(0), Asset.Tbd(0), '0.000001 VESTS')
    except Exception as e:
        message = str(e)
        logger.info(message)
        found = message.find('Cannot claim that much VESTS')
        assert found != -1
