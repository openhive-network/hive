from test_tools import Account, logger, World

def test_order(world):
    init_node = world.create_init_node()
    init_node.run()

    wallet = init_node.attach_wallet()

    #**************************************************************
    response = wallet.api.create_account('initminer', 'alice', '{}')
    assert 'result' in response

    #**************************************************************
    response = wallet.api.transfer('initminer', 'alice', '77.000 TESTS', 'lime')
    assert 'result' in response

    #**************************************************************
    response = wallet.api.transfer_to_vesting('initminer', 'alice', '500.000 TESTS')
    assert 'result' in response

    #**************************************************************
    response = wallet.api.create_order('alice', 666, '7.000 TESTS', '1.000 TBD', False, 3600 )
    _result = response['result']

    _ops = _result['operations']
    _op = _ops[0]

    _value = _op[1]
    assert _value['amount_to_sell'] == '7.000 TESTS'
    assert _value['min_to_receive'] == '1.000 TBD'

    #**************************************************************
    response = wallet.api.create_order('alice', 667, '8.000 TESTS', '2.000 TBD', False, 3600 )
    _result = response['result']
    assert 'operations' in _result

    #**************************************************************
    response = wallet.api.get_order_book(5)
    _result = response['result']

    _asks = _result['asks']

    assert len(_asks) == 2

    def check_ask( node, base, quote ):
      assert 'order_price' in node
      _order_price = node['order_price']

      assert 'base' in _order_price
      _order_price['base'] == base

      assert 'quote' in _order_price
      _order_price['quote'] == quote

    check_ask( _asks[0], '7.000 TESTS', '1.000 TBD' )
    check_ask( _asks[1], '8.000 TESTS', '2.000 TBD' )

    #**************************************************************
    response = wallet.api.get_open_orders('alice')
    _result = response['result']

    assert len(_result) == 2

    def check_sell_price( node, base, quote ):
      assert 'sell_price' in node
      _sell_price = node['sell_price']

      assert 'base' in _sell_price
      _sell_price['base'] == base

      assert 'quote' in _sell_price
      _sell_price['quote'] == quote

    check_sell_price( _result[0], '7.000 TESTS', '1.000 TBD' )
    check_sell_price( _result[1], '8.000 TESTS', '2.000 TBD' )

    #**************************************************************
    response = wallet.api.cancel_order('alice', 667)
    _result = response['result']

    assert 'operations' in _result
    assert len(_result['operations']) == 1

    #**************************************************************
    response = wallet.api.get_order_book(5)
    _result = response['result']

    _asks = _result['asks']
    assert len(_asks) == 1

    #**************************************************************
    response = wallet.api.get_open_orders('alice')
    _result = response['result']

    assert len(_result) == 1

    check_sell_price( _result[0], '7.000 TESTS', '1.000 TBD' )