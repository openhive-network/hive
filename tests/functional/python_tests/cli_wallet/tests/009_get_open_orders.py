#!/usr/bin/python3

import time

from utils.test_utils import *
from utils.cmd_args   import args
from utils.cli_wallet import CliWallet
from utils.logger     import log, init_logger

if __name__ == "__main__":
    with Test(__file__):
        with CliWallet( args ) as wallet:
            creator, user = make_user_for_tests(wallet)
            result_before = wallet.get_open_orders(user)['result']
            assert(len(result_before) == 0)

            log.info( "testing buy order :10.000 TESTS for 1000.000 TBD created by user {}".format( user ) )
            wallet.create_order(user, "1", "10.000 TESTS", "1000.000 TBD", "false", "9999", "true")
            result_sell = wallet.get_open_orders(user)['result']
            assert(len(result_sell) == 1)
            assert(result_sell[0]['orderid'] == 1)
            assert(result_sell[0]['seller'] == user)
            assert(result_sell[0]['for_sale'] == 10000)
            assert(result_sell[0]['sell_price']['base']['amount'] == '10000')
            assert(result_sell[0]['sell_price']['quote']['amount'] == '1000000')

            log.info( "testing buy order :10.000 TBD for 1000.000 TESTS created by user {}".format( user ) )
            wallet.create_order(user, "2", "10.000 TBD", "1000.000 TESTS", "false", "9999", "true")
            result_buy = wallet.get_open_orders(user)['result']
            assert(len(result_buy) == 2)
            assert(result_buy[1]['orderid'] == 2)
            assert(result_buy[1]['seller'] == user)
            assert(result_buy[1]['for_sale'] == 10000)
            assert(result_buy[1]['sell_price']['base']['amount'] == '10000')
            assert(result_buy[1]['sell_price']['quote']['amount'] == '1000000')

