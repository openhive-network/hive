import test_tools as tt

from .utilities import check_keys, create_accounts


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
def test_different_false_cases(wallet):
    try:
        response = wallet.api.get_account('not-exists')
    except Exception as e:
        message = str(e)
        assert message.find('Account does not exist') != -1

    try:
        response = wallet.api.create_account('initminer')
    except Exception as e:
        message = str(e)
        assert message.find('create_account() missing 2 required positional arguments: \'new_account_name\' and \'json_meta\'') != -1

    create_accounts( wallet, 'initminer', ['alice', 'bob'] )

    try:
        response = wallet.api.create_proposal('bob', 'bob', '2031-01-01T00:00:00', '2031-06-01T00:00:00', tt.Asset.Tbd(111), 'this is proposal', 'hello-world')
    except Exception as e:
        message = str(e)
        assert message.find('Proposal permlink must point to the article posted by creator or receiver') != -1

    try:
        response = wallet.api.update_proposal(1, "initminer", "10.000 TBD", "avocado", "mango", '2022-10-19T23:41:54')
    except Exception as e:
        message = str(e)
        assert message.find('Cannot edit a proposal because the proposal with given id doesn\'t exist') != -1

    try:
        response = wallet.api.post_comment('bob', 'hello-world', '', 'xyz', 'something bout world', 'just nothing', '{}')
    except Exception as e:
        message = str(e)
        assert message.find('Account: bob has 0 RC') != -1

    wallet.api.transfer_to_vesting('initminer', 'bob', tt.Asset.Test(100))

    wallet.api.post_comment('bob', 'hello-world', '', 'xyz', 'something about world', 'just nothing', '{}')

    try:
        response = wallet.api.create_proposal('bob', 'bob', '2031-01-01T00:00:00', '2031-06-01T00:00:00', tt.Asset.Tbd(111), 'this is proposal', 'hello-world')
    except Exception as e:
        message = str(e)
        assert message.find('Account bob does not have sufficient funds for balance adjustment') != -1

    try:
        response = wallet.api.vote('bob', 'bob', 'hello-world', 101)
    except Exception as e:
        message = str(e)
        assert message.find('Weight must be between -100 and 100 and not 0') != -1

    try:
        response = wallet.api.vote('alice', 'bob', 'hello-world', 99)
    except Exception as e:
        message = str(e)
        assert message.find('Account: alice has 0 RC, needs 1 RC') != -1

    try:
        with wallet.in_single_transaction(broadcast=False) as transaction:
            wallet.api.transfer('bob', 'alice', tt.Asset.Tbd(199.148), 'banana')
            wallet.api.post_comment('alice', 'hello-world2', '', 'xyz2', 'something about world2', 'just nothing2', '{}')
    except Exception as e:
        message = str(e)
        assert message.find('required_active.size()') != -1

def test_complex(wallet):
    create_accounts( wallet, 'initminer', ['alice', 'bob', 'carol', 'dan'] )

    wallet.api.transfer_to_vesting('initminer', 'bob', tt.Asset.Test(100))

    wallet.api.post_comment('bob', 'hello-world', '', 'xyz', 'something about world', 'just nothing', '{}')

    wallet.api.transfer('initminer', 'bob', tt.Asset.Tbd(788.543), 'avocado')

    wallet.api.create_proposal('bob', 'bob', '2031-01-01T00:00:00', '2031-06-01T00:00:00', tt.Asset.Tbd(111), 'this is proposal', 'hello-world')

    with wallet.in_single_transaction(broadcast=False) as transaction:
        wallet.api.transfer('bob', 'alice', tt.Asset.Tbd(199.148), 'banana')
        wallet.api.transfer('bob', 'alice', tt.Asset.Tbd(100.001), 'cherry')
        wallet.api.transfer('initminer', 'alice', tt.Asset.Tbd(1), 'aloes')
        wallet.api.transfer('initminer', 'carol', tt.Asset.Tbd(199.001), 'pumpkin')
        wallet.api.transfer('initminer', 'dan', tt.Asset.Tbd(198.002), 'beetroot')
        wallet.api.transfer_to_vesting('initminer', 'carol', tt.Asset.Test(100))
        wallet.api.transfer_to_vesting('initminer', 'alice', tt.Asset.Test(100))
        wallet.api.transfer_to_vesting('initminer', 'dan', tt.Asset.Test(100))

    _result_trx_response = transaction.get_response()

    response = wallet.api.sign_transaction(_result_trx_response)

    assert len(response['operations']) == 8

    with wallet.in_single_transaction(broadcast=False) as transaction:
        wallet.api.post_comment('alice', 'hello-world2', '', 'xyz2', 'something about world2', 'just nothing2', '{}')
        wallet.api.post_comment('carol', 'hello-world3', '', 'xyz3', 'something about world3', 'just nothing3', '{}')
        wallet.api.post_comment('dan', 'hello-world4', '', 'xyz4', 'something about world4', 'just nothing4', '{}')
        wallet.api.vote('carol', 'alice', 'hello-world2', 99)
        wallet.api.vote('dan', 'carol', 'hello-world3', 98)
        wallet.api.vote('alice', 'dan', 'hello-world4', 97)

    _result_trx_response = transaction.get_response()

    response = wallet.api.sign_transaction(_result_trx_response)

    assert len(response['operations']) == 6

    with wallet.in_single_transaction() as transaction:
        for cnt in range(10):
            wallet.api.create_account_with_keys('initminer', 'account-'+str(cnt), '{}', key, key2, key2, key)

    trx_response = transaction.get_response()

    assert len(trx_response['operations']) == 10

    with wallet.in_single_transaction() as transaction:
        wallet.api.update_account('alice', '{}', key2, key, key, key)
        wallet.api.update_account('bob', '{}', key, key2, key, key)
        wallet.api.update_account('carol', '{}', key, key, key2, key)
        wallet.api.update_account('dan', '{}', key, key, key, key2)

    _result = wallet.api.get_accounts(['alice', 'bob', 'carol', 'dan'])
    assert len(_result) == 4
    check_keys( _result[0], key2, key, key, key )
    check_keys( _result[1], key, key2, key, key )
    check_keys( _result[2], key, key, key2, key )
    check_keys( _result[3], key, key, key, key2 )
