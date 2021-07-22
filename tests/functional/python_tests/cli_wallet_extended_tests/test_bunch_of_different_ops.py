from test_tools import Account, logger, World

def check_key( node_name, result, key ):
    _node = result[node_name]
    _key_auths = _node['key_auths']
    assert len(_key_auths) == 1
    __key_auths = _key_auths[0]
    assert len(__key_auths) == 2
    __key_auths[0] == key

def check_keys( result, key_owner, key_active, key_posting, key_memo ):
    check_key( 'owner', result, key_owner )
    check_key( 'active', result, key_active )
    check_key( 'posting', result, key_posting )
    assert 'memo_key' in result and result['memo_key'] == key_memo

key = 'TST8grZpsMPnH7sxbMVZHWEu1D26F3GwLW1fYnZEuwzT4Rtd57AER'
key2 = 'TST7QbuPFWyi7Kxtq6i1EaHNHZHEG2JyB61kPY1x7VvjxyHb7btfg'

'''
  This test designed for different operations in different order so as to do some operations similar to main-net.
  These operation should be done one by one.

  Features:
  a) 1 operation in 1 transaction
  b) N operations in 1 transaction
  c) false tests:
    - an exception is thrown by node
    - an exception is thrown by cli_wallet
'''
def test_complex():
    with World() as world:
        init_node = world.create_init_node()
        init_node.run()

        wallet = init_node.attach_wallet()

        #**************************************************************
        try:
            response = wallet.api.get_account('not-exists')
        except Exception as e:
            message = str(e)
            assert message.find('Unknown account') != -1

        #**************************************************************
        try:
            response = wallet.api.create_account('initminer')
        except Exception as e:
            message = str(e)
            assert message.find('create_account() missing 2 required positional arguments: \'new_account_name\' and \'json_meta\'') != -1

        #**************************************************************
        response = wallet.api.create_account('initminer', 'alice', '{}')
        assert 'result' in response

        #**************************************************************
        response = wallet.api.create_account('initminer', 'bob', '{}')
        assert 'result' in response

        #**************************************************************
        response = wallet.api.create_account('initminer', 'carol', '{}')
        assert 'result' in response

        #**************************************************************
        response = wallet.api.create_account('initminer', 'dan', '{}')
        assert 'result' in response

        #**************************************************************
        try:
            response = wallet.api.create_proposal('bob', 'bob', '2031-01-01T00:00:00', '2031-06-01T00:00:00', '111.000 TBD', 'this is proposal', 'hello-world')
        except Exception as e:
            message = str(e)
            assert message.find('Proposal permlink must point to the article posted by creator or receiver') != -1

        #**************************************************************
        try:
            response = wallet.api.post_comment('bob', 'hello-world', '', 'xyz', 'something bout world', 'just nothing', '{}')
        except Exception as e:
            message = str(e)
            assert message.find('Account: bob has 0 RC') != -1

        #**************************************************************
        response = wallet.api.transfer_to_vesting('initminer', 'bob', '100.000 TESTS')
        assert 'result' in response

        #**************************************************************
        response = wallet.api.post_comment('bob', 'hello-world', '', 'xyz', 'something about world', 'just nothing', '{}')
        assert 'result' in response

        #**************************************************************
        try:
            response = wallet.api.create_proposal('bob', 'bob', '2031-01-01T00:00:00', '2031-06-01T00:00:00', '111.000 TBD', 'this is proposal', 'hello-world')
        except Exception as e:
            message = str(e)
            assert message.find('Account bob does not have sufficient funds for balance adjustment') != -1

        #**************************************************************
        response = wallet.api.transfer('initminer', 'bob', '788.543 TBD', 'avocado')
        assert 'result' in response

        #**************************************************************
        response = wallet.api.create_proposal('bob', 'bob', '2031-01-01T00:00:00', '2031-06-01T00:00:00', '111.000 TBD', 'this is proposal', 'hello-world')
        assert 'result' in response

        #**************************************************************
        try:
            response = wallet.api.vote('alice', 'bob', 'hello-world', 101)
        except Exception as e:
            message = str(e)
            assert message.find('Weight must be between -100 and 100 and not 0') != -1

        #**************************************************************
        try:
            response = wallet.api.vote('alice', 'bob', 'hello-world', 99)
        except Exception as e:
            message = str(e)
            assert message.find('Account: alice has 0 RC, needs 3 RC') != -1

        #**************************************************************
        try:
            with wallet.in_single_transaction(broadcast=False) as transaction:
                wallet.api.transfer('bob', 'alice', '199.148 TBD', 'banana')
                wallet.api.post_comment('alice', 'hello-world2', '', 'xyz2', 'something about world2', 'just nothing2', '{}')
        except Exception as e:
            message = str(e)
            wallet.logger.info("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^2")
            wallet.logger.info(message)
            wallet.logger.info("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^2")
            assert message.find('required_active.size()') != -1

        #**************************************************************
        with wallet.in_single_transaction(broadcast=False) as transaction:
            wallet.api.transfer('bob', 'alice', '199.148 TBD', 'banana')
            wallet.api.transfer('bob', 'alice', '100.001 TBD', 'cherry')
            wallet.api.transfer('initminer', 'alice', '1.000 TBD', 'aloes')
            wallet.api.transfer('initminer', 'carol', '199.001 TBD', 'pumpkin')
            wallet.api.transfer('initminer', 'dan', '198.002 TBD', 'beetroot')
            wallet.api.transfer_to_vesting('initminer', 'carol', '100.000 TESTS')
            wallet.api.transfer_to_vesting('initminer', 'alice', '100.000 TESTS')
            wallet.api.transfer_to_vesting('initminer', 'dan', '100.000 TESTS')

        trx_response = transaction.get_response()

        _result_trx_response = trx_response['result']

        #**************************************************************
        response = wallet.api.sign_transaction(_result_trx_response)
        _result = response['result']

        assert len(_result['operations']) == 8

        #**************************************************************
        with wallet.in_single_transaction(broadcast=False) as transaction:
            wallet.api.post_comment('alice', 'hello-world2', '', 'xyz2', 'something about world2', 'just nothing2', '{}')
            wallet.api.post_comment('carol', 'hello-world3', '', 'xyz3', 'something about world3', 'just nothing3', '{}')
            wallet.api.post_comment('dan', 'hello-world4', '', 'xyz4', 'something about world4', 'just nothing4', '{}')
            wallet.api.vote('carol', 'alice', 'hello-world2', 99)
            wallet.api.vote('dan', 'carol', 'hello-world3', 98)
            wallet.api.vote('alice', 'dan', 'hello-world4', 97)

        trx_response = transaction.get_response()
        _result_trx_response = trx_response['result']

        #**************************************************************
        response = wallet.api.sign_transaction(_result_trx_response)

        _result = response['result']

        assert len(_result['operations']) == 6

        #**************************************************************
        with wallet.in_single_transaction() as transaction:
            for cnt in range(10):
                wallet.api.create_account_with_keys('initminer', 'account-'+str(cnt), '{}', key, key2, key2, key)

        trx_response = transaction.get_response()

        _result = trx_response['result']

        assert len(_result['operations']) == 10

        #**************************************************************
        with wallet.in_single_transaction() as transaction:
            wallet.api.update_account('alice', '{}', key2, key, key, key)
            wallet.api.update_account('bob', '{}', key, key2, key, key)
            wallet.api.update_account('carol', '{}', key, key, key2, key)
            wallet.api.update_account('dan', '{}', key, key, key, key2)

        trx_response = transaction.get_response()

        assert 'result' in trx_response

        #**************************************************************
        response = wallet.api.get_accounts(['alice', 'bob', 'carol', 'dan'])

        _result = response['result']
        assert len(_result) == 4

        check_keys( _result[0], key2, key, key, key )
        check_keys( _result[1], key, key2, key, key )
        check_keys( _result[2], key, key, key2, key )
        check_keys( _result[3], key, key, key, key2 )
