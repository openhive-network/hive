from test_tools import Account, logger, World

def test_order():
    with World() as world:
        init_node = world.create_init_node()
        init_node.config.plugin.append('database_api')
        init_node.config.plugin.append('network_broadcast_api')
        init_node.config.plugin.append('market_history_api')
        init_node.run()

        wallet = init_node.attach_wallet()

        #**************************************************************
        logger.info('create_account...')
        response = wallet.api.create_account('initminer', 'alice', '{}')
        logger.info(response)

        #**************************************************************
        logger.info('transfer...')
        response = wallet.api.transfer('initminer', 'alice', '77.000 TESTS', 'lime')
        logger.info(response)

        #**************************************************************
        logger.info('transfer_to_vesting...')
        response = wallet.api.transfer_to_vesting('initminer', 'alice', '500.000 TESTS')
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        assert 'ref_block_num' in _result
        block_number = _result['ref_block_num'] + 1

        #**************************************************************
        logger.info('create_order...')
        response = wallet.api.create_order('alice', 666, '7.000 TESTS', '1.000 TBD', False, 3600 )
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        assert 'operations' in _result
        _ops = _result['operations']

        assert len(_ops) == 1
        _op = _ops[0]

        assert 'value' in _op
        _value = _op['value']

        assert 'amount_to_sell' in _value
        assert _value['amount_to_sell'] == '7.000 TESTS'

        assert 'min_to_receive' in _value
        assert _value['min_to_receive'] == '1.000 TBD'

        #**************************************************************
        logger.info('create_order...')
        response = wallet.api.create_order('alice', 667, '8.000 TESTS', '2.000 TBD', False, 3600 )
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        assert 'operations' in _result

        #**************************************************************
        logger.info('get_order_book...')
        response = wallet.api.get_order_book(5)
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        assert 'asks' in _result
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
        logger.info('get_open_orders...')
        response = wallet.api.get_open_orders('alice')
        logger.info(response)

        assert 'result' in response
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
        logger.info('cancel_order...')
        response = wallet.api.cancel_order('alice', 667)
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        assert 'operations' in _result
        _ops = _result['operations']

        #**************************************************************
        logger.info('get_order_book...')
        response = wallet.api.get_order_book(5)
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        assert 'asks' in _result
        _asks = _result['asks']

        assert len(_asks) == 1

        #**************************************************************
        logger.info('get_open_orders...')
        response = wallet.api.get_open_orders('alice')
        logger.info(response)

        assert 'result' in response
        _result = response['result']

        assert len(_result) == 1

        check_sell_price( _result[0], '7.000 TESTS', '1.000 TBD' )