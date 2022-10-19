import test_tools as tt


def test_witness(wallet):
    wallet.api.create_account('initminer', 'alice', '{}')

    wallet.api.transfer_to_vesting('initminer', 'alice', tt.Asset.Test(500))

    _result = wallet.api.get_active_witnesses(False)

    assert len(_result) == 1
    assert _result[0] == 'initminer'

    _result = wallet.api.list_witnesses('a', 4)
    assert len(_result) == 1
    assert _result[0] == 'initminer'

    wallet.api.update_witness('alice', 'http:\\url.html', 'TST6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4', { 'account_creation_fee':'2789.030 TESTS', 'maximum_block_size' : 131072, 'hbd_interest_rate' : 1000 } )

    _result = wallet.api.get_active_witnesses(False)

    assert len(_result) == 1
    assert _result[0] == 'initminer'

    _result = wallet.api.list_witnesses('a', 4)
    assert len(_result) == 2
    assert _result[0] == 'alice'
    assert _result[1] == 'initminer'

    _result = wallet.api.get_witness('alice')
    assert _result['owner'] == 'alice'

    _props = _result['props']
    assert _props['account_creation_fee'] == tt.Asset.Test(2789.03)

    response = wallet.api.publish_feed('alice', {"base":"1.167 TBD", "quote":"1.111 TESTS"})

    _ops = response['operations']

    assert _ops[0][0] == 'feed_publish'

    _exchange_rate = _ops[0][1]['exchange_rate']
    assert _exchange_rate['base'] == tt.Asset.Tbd(1.167)
    assert _exchange_rate['quote'] == tt.Asset.Test(1.111)

    response = wallet.api.vote_for_witness('initminer', 'alice', True)

    _ops = response['operations']

    assert _ops[0][0] == 'account_witness_vote'

    assert _ops[0][1]['account'] == 'initminer'
    assert _ops[0][1]['witness'] == 'alice'
    assert _ops[0][1]['approve'] == True

    response = wallet.api.set_voting_proxy('alice', 'initminer')

    _ops = response['operations']

    assert _ops[0][0] == 'account_witness_proxy'

    assert _ops[0][1]['account'] == 'alice'
    assert _ops[0][1]['proxy'] == 'initminer'