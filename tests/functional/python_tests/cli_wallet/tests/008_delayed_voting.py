#!/usr/bin/python3

import time
import json

#from ... import utils
from utils.test_utils import *
from utils.cmd_args   import args
from utils.cli_wallet import CliWallet
from utils.logger     import log, init_logger

def get_votes( wallet, account_name ):
    dv_items = wallet.get_account( account_name )["result"]["delayed_votes"]

    val = 0
    for item in dv_items:
        assert( "val" in item )
        val += int( item[ "val" ] )

    return val

if __name__ == "__main__":
    with Test(__file__):
        with CliWallet(args) as wallet:
            creator, receiver = make_user_for_tests(wallet)
            log.info( "actors:  creator: {0} receiver: ".format( creator, receiver ) )

            #=================Adding delayed votes=================
            before_val = get_votes( wallet, receiver )

            #=================Vest liquid again=================
            wallet.transfer_to_vesting( creator, receiver, "1.000 TESTS", "true" )

            #=================Adding delayed votes=================
            after_val = get_votes( wallet, receiver )

            log.info( "before_val: {0} after_val: {1}".format( before_val, after_val ) )

            #=================Checking changes=================
            assert( after_val > before_val )
