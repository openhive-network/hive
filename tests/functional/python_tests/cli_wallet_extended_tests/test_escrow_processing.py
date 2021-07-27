from test_tools import Account, logger, World, Asset

def test_escrow(world):
    init_node = world.create_init_node()
    init_node.run()

    wallet = init_node.attach_wallet()

    #**************************************************************
    wallet.api.create_account('initminer', 'alice', '{}')

    #**************************************************************
    wallet.api.transfer('initminer', 'alice', Asset.Test(200), 'avocado')

    #**************************************************************
    wallet.api.transfer('initminer', 'alice', Asset.Tbd(100), 'banana')

    #**************************************************************
    wallet.api.transfer_to_vesting('initminer', 'alice', Asset.Test(50))

    #**************************************************************
    wallet.api.create_account('alice', 'bob', '{}')

    #**************************************************************
    wallet.api.create_account('alice', 'carol', '{}')

    #**************************************************************
    wallet.api.transfer('alice', 'bob', Asset.Test(50), 'lemon')

    #**************************************************************
    wallet.api.transfer_to_vesting('alice', 'bob', Asset.Test(25))

    #**************************************************************
    response = wallet.api.get_accounts(['alice', 'bob', 'carol'])

    _result = response['result']
    assert len(_result) == 3

    _alice = _result[0]
    assert _alice['balance'] == Asset.Test(125)
    assert _alice['hbd_balance'] == Asset.Tbd(100)

    _bob = _result[1]
    assert _bob['balance'] == Asset.Test(50)
    assert _bob['hbd_balance'] == Asset.Tbd(0)

    _carol = _result[2]
    assert _carol['balance'] == Asset.Test(0)
    assert _carol['hbd_balance'] == Asset.Tbd(0)

    # #**************************************************************
    wallet.api.escrow_transfer('alice', 'bob', 'carol', 99, Asset.Tbd(1), Asset.Test(2), Asset.Tbd(5), '2029-06-02T00:00:00', '2029-06-02T01:01:01', '{}')

    #**************************************************************
    response = wallet.api.get_accounts(['alice', 'bob', 'carol'])

    _result = response['result']

    assert len(_result) == 3

    _alice = _result[0]
    assert _alice['balance'] == Asset.Test(123)
    assert _alice['hbd_balance'] == Asset.Tbd(94)

    _bob = _result[1]
    assert _bob['balance'] == Asset.Test(50)
    assert _bob['hbd_balance'] == Asset.Tbd(0)

    _carol = _result[2]
    assert _carol['balance'] == Asset.Test(0)
    assert _carol['hbd_balance'] == Asset.Tbd(0)

    #**************************************************************
    wallet.api.transfer_to_vesting('initminer', 'carol', Asset.Test(50))

    #**************************************************************
    wallet.api.escrow_approve('alice', 'bob', 'carol', 'carol', 99, True)

    #**************************************************************
    response = wallet.api.get_accounts(['alice', 'bob', 'carol'])

    _result = response['result']

    assert len(_result) == 3

    _alice = _result[0]
    assert _alice['balance'] == Asset.Test(123)
    assert _alice['hbd_balance'] == Asset.Tbd(94)

    _bob = _result[1]
    assert _bob['balance'] == Asset.Test(50)
    assert _bob['hbd_balance'] == Asset.Tbd(0)

    _carol = _result[2]
    assert _carol['balance'] == Asset.Test(0)
    assert _carol['hbd_balance'] == Asset.Tbd(0)

    #**************************************************************
    wallet.api.escrow_approve('alice', 'bob', 'carol', 'bob', 99, True)

    #**************************************************************
    response = wallet.api.get_accounts(['alice', 'bob', 'carol'])

    _result = response['result']

    assert len(_result) == 3

    _alice = _result[0]
    assert _alice['balance'] == Asset.Test(123)
    assert _alice['hbd_balance'] == Asset.Tbd(94)

    _bob = _result[1]
    assert _bob['balance'] == Asset.Test(50)
    assert _bob['hbd_balance'] == Asset.Tbd(0)

    _carol = _result[2]
    assert _carol['balance'] == Asset.Test(0)
    assert _carol['hbd_balance'] == Asset.Tbd(5)

    #**************************************************************
    wallet.api.escrow_dispute('alice', 'bob', 'carol', 'alice', 99)

    #**************************************************************
    response = wallet.api.get_accounts(['alice', 'bob', 'carol'])

    _result = response['result']

    assert len(_result) == 3

    _alice = _result[0]
    assert _alice['balance'] == Asset.Test(123)
    assert _alice['hbd_balance'] == Asset.Tbd(94)

    _bob = _result[1]
    assert _bob['balance'] == Asset.Test(50)
    assert _bob['hbd_balance'] == Asset.Tbd(0)

    _carol = _result[2]
    assert _carol['balance'] == Asset.Test(0)
    assert _carol['hbd_balance'] == Asset.Tbd(5)

    #**************************************************************
    wallet.api.escrow_release('alice', 'bob', 'carol', 'carol', 'bob', 99, Asset.Tbd(1), Asset.Test(2))

    #**************************************************************
    response = wallet.api.get_accounts(['alice', 'bob', 'carol'])

    _result = response['result']

    assert len(_result) == 3

    _alice = _result[0]
    assert _alice['balance'] == Asset.Test(123)
    assert _alice['hbd_balance'] == Asset.Tbd(94)

    _bob = _result[1]
    assert _bob['balance'] == Asset.Test(52)
    assert _bob['hbd_balance'] == Asset.Tbd(1)

    _carol = _result[2]
    assert _carol['balance'] == Asset.Test(0)
    assert _carol['hbd_balance'] == Asset.Tbd(5)
