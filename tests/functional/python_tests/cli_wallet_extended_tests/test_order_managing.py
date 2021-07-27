from test_tools import Account, logger, World, Asset

def test_order(world):
    init_node = world.create_init_node()
    init_node.run()

    wallet = init_node.attach_wallet()

    #**************************************************************
    wallet.api.create_account('initminer', 'alice', '{}')

    #**************************************************************
    wallet.api.transfer('initminer', 'alice', Asset.Test(77), 'lime')

    #**************************************************************
    wallet.api.transfer_to_vesting('initminer', 'alice', Asset.Test(500))

    #**************************************************************
    response = wallet.api.create_order('alice', 666, Asset.Test(7), Asset.Tbd(1), False, 3600 )
    _result = response['result']

    _ops = _result['operations']
    _op = _ops[0]

    _value = _op[1]
    assert _value['amount_to_sell'] == Asset.Test(7)
    assert _value['min_to_receive'] == Asset.Tbd(1)

    #**************************************************************
    response = wallet.api.create_order('alice', 667, Asset.Test(8), Asset.Tbd(2), False, 3600 )
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

    check_ask( _asks[0], Asset.Test(7), Asset.Tbd(1) )
    check_ask( _asks[1], Asset.Test(8), Asset.Tbd(2) )

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

    check_sell_price( _result[0], Asset.Test(7), Asset.Tbd(1) )
    check_sell_price( _result[1], Asset.Test(8), Asset.Tbd(2) )

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

    check_sell_price( _result[0], Asset.Test(7), Asset.Tbd(1) )