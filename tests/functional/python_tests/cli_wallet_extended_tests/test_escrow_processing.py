from test_tools import Account, logger, World

def test_escrow():
    with World() as world:
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
        response = wallet.api.create_account('alice', 'carol', '{}')
        assert 'result' in response

        #**************************************************************
        response = wallet.api.transfer('alice', 'bob', '50.000 TESTS', 'lemon')
        assert 'result' in response

        #**************************************************************
        response = wallet.api.transfer_to_vesting('alice', 'bob', '25.000 TESTS')
        assert 'result' in response

        #**************************************************************
        response = wallet.api.get_accounts(['alice', 'bob', 'carol'])

        _result = response['result']
        assert len(_result) == 3

        _alice = _result[0]
        assert _alice['balance'] == '125.000 TESTS'
        assert _alice['hbd_balance'] == '100.000 TBD'

        _bob = _result[1]
        assert _bob['balance'] == '50.000 TESTS'
        assert _bob['hbd_balance'] == '0.000 TBD'

        _carol = _result[2]
        assert _carol['balance'] == '0.000 TESTS'
        assert _carol['hbd_balance'] == '0.000 TBD'

        # #**************************************************************
        response = wallet.api.escrow_transfer('alice', 'bob', 'carol', 99, '1.000 TBD', '2.000 TESTS', '5.000 TBD', '2029-06-02T00:00:00', '2029-06-02T01:01:01', '{}')
        assert 'result' in response

        #**************************************************************
        response = wallet.api.get_accounts(['alice', 'bob', 'carol'])
        _result = response['result']

        assert len(_result) == 3

        _alice = _result[0]
        assert _alice['balance'] == '123.000 TESTS'
        assert _alice['hbd_balance'] == '94.000 TBD'

        _bob = _result[1]
        assert _bob['balance'] == '50.000 TESTS'
        assert _bob['hbd_balance'] == '0.000 TBD'

        _carol = _result[2]
        assert _carol['balance'] == '0.000 TESTS'
        assert _carol['hbd_balance'] == '0.000 TBD'

        #**************************************************************
        response = wallet.api.transfer_to_vesting('initminer', 'carol', '50.000 TESTS')
        assert 'result' in response

        #**************************************************************
        response = wallet.api.escrow_approve('alice', 'bob', 'carol', 'carol', 99, True)
        assert 'result' in response

        #**************************************************************
        response = wallet.api.get_accounts(['alice', 'bob', 'carol'])
        _result = response['result']

        assert len(_result) == 3

        _alice = _result[0]
        assert _alice['balance'] == '123.000 TESTS'
        assert _alice['hbd_balance'] == '94.000 TBD'

        _bob = _result[1]
        assert _bob['balance'] == '50.000 TESTS'
        assert _bob['hbd_balance'] == '0.000 TBD'

        _carol = _result[2]
        assert _carol['balance'] == '0.000 TESTS'
        assert _carol['hbd_balance'] == '0.000 TBD'

        #**************************************************************
        response = wallet.api.escrow_approve('alice', 'bob', 'carol', 'bob', 99, True)
        assert 'result' in response

        #**************************************************************
        response = wallet.api.get_accounts(['alice', 'bob', 'carol'])
        _result = response['result']

        assert len(_result) == 3

        _alice = _result[0]
        assert _alice['balance'] == '123.000 TESTS'
        assert _alice['hbd_balance'] == '94.000 TBD'

        _bob = _result[1]
        assert _bob['balance'] == '50.000 TESTS'
        assert _bob['hbd_balance'] == '0.000 TBD'

        _carol = _result[2]
        assert _carol['balance'] == '0.000 TESTS'
        assert _carol['hbd_balance'] == '5.000 TBD'

        #**************************************************************
        response = wallet.api.escrow_dispute('alice', 'bob', 'carol', 'alice', 99)
        assert 'result' in response

        #**************************************************************
        response = wallet.api.get_accounts(['alice', 'bob', 'carol'])

        _result = response['result']
        assert len(_result) == 3

        _alice = _result[0]
        assert _alice['balance'] == '123.000 TESTS'
        assert _alice['hbd_balance'] == '94.000 TBD'

        _bob = _result[1]
        assert _bob['balance'] == '50.000 TESTS'
        assert _bob['hbd_balance'] == '0.000 TBD'

        _carol = _result[2]
        assert _carol['balance'] == '0.000 TESTS'
        assert _carol['hbd_balance'] == '5.000 TBD'

        #**************************************************************
        response = wallet.api.escrow_release('alice', 'bob', 'carol', 'carol', 'bob', 99, '1.000 TBD', '2.000 TESTS')
        assert 'result' in response

        #**************************************************************
        response = wallet.api.get_accounts(['alice', 'bob', 'carol'])
        _result = response['result']

        assert len(_result) == 3

        _alice = _result[0]
        assert _alice['balance'] == '123.000 TESTS'
        assert _alice['hbd_balance'] == '94.000 TBD'

        _bob = _result[1]
        assert _bob['balance'] == '52.000 TESTS'
        assert _bob['hbd_balance'] == '1.000 TBD'

        _carol = _result[2]
        assert _carol['balance'] == '0.000 TESTS'
        assert _carol['hbd_balance'] == '5.000 TBD'
